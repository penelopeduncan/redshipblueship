/**
 * @file test_warp_song_tracker.c
 * @brief #1169 lane XW11: the added warp songs' tracker rules, their names, and
 *        the check trackers' travel facts from the frozen world.
 *
 *   warp-song-tracker (redship tier, ROM-free, no generation, no CVar store):
 *     A. Combo_AddedSongRow / Combo_AddedSongHeld (warp_song_items.h): no row with
 *        the rule off whatever is owned; the rule on shows it, held only when
 *        owned; a shared bit alone or a bit that is not an extension shows
 *        nothing; the shared owner counts only under its frozen shared bit.
 *     B. Combo_ForeignGameTag: "" after a name that already ends with its own
 *        game's tag, the tag after any other (and after the other game's tag).
 *     C. Combo_TrackerTravel_IntoOoT / _IntoMM (warp_song_tracker.h), pure: every
 *        term is needed, and only the song's own bit is given.
 *     D. The two readers over saves authored in each game's TU: OoT's Song of
 *        Soaring (owned, an ocarina and C-down, C-up, C-left) and MM's playable
 *        warp songs (the Ocarina of Time and each song's buttons).
 *     E. The facts now, from the frozen world (each game's adapter registered by
 *        its production Register): a frozen MM half, the ownership byte, the
 *        pairing and the frozen word give OoT's tracker the song; each missing
 *        input gives 0. A frozen OoT child half gives MM's tracker the Soaring
 *        fact; an adult or a closed crossing gives 0. The destination is the
 *        frozen record's, 0 without one.
 *     F. The engines' tracker arms, outside any round: OoT's root warp accessor
 *        opens for an armed song to a credited record destination and stays
 *        closed for a grotto destination, an unset record, another song and once
 *        disarmed; MM's Soaring accessor answers the arm and counts no round hit.
 *     G. ComboCheckRowItemText (the combo pane's row text), behaviourally: an MM
 *        song in OoT and OoT's Soaring in MM carry one tag; a plain item keeps
 *        its tag; the item's own game shows none. So a site handed the host's
 *        game instead of the item's fails.
 *     S. Function-scoped source anchors: every name site calls the helper and no
 *        longer appends a literal tag; both tracker entry points take the
 *        crossing from their first search, arm the facts for a second and clear
 *        them; the engines read the arm only outside a round, OoT's with the
 *        record's destinations; nothing else arms them; both Register functions
 *        register their reader.
 *
 * The Combo_Xw11TestTracker* bridge below sets the frozen world for the rando-tier
 * legs that drive the REAL tracker entry points (leg W of TrackerAvailabilityArrival
 * and of MMTrackerInLogicArrival); this row does not call it.
 *
 * FILE SCOPE (compiled as C++, #included into test_runner.cpp).
 */

#include "../ComboTrackerWindow.h" // ComboGui::ComboCheckRowItemText
#include "../combo_logic.h"
#include "../combo_tracker_view.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../game.h"
#include "../mm_warp_song_play.h"
#include "../test_runner.h"
#include "../warp_song_items.h"
#include "../warp_song_tracker.h"
#include "../warp_songs_mm.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" {
// Both ItemAdapterSingleExe.cpp files: the production registrations, the readers and the authoring seams.
void OoT_ItemAdapter_Register(void);
void MM_ItemAdapter_Register(void);
int OoT_ItemAdapter_TestTrackerSoaring(const void* buf, size_t size, uint8_t* owned, uint8_t* playable);
uint32_t MM_ItemAdapter_TestTrackerPlayable(const void* buf, size_t size);
int OoT_ItemAdapter_TestAuthorSoaringWorld(void* buf, size_t size, int soaringOwned, int ocarina, int buttons,
                                           int linkAge);
int MM_ItemAdapter_TestAuthorWarpSongWorld(void* buf, size_t size, int ocarina, unsigned heldMask);
// ComboLogicEngineOoT.cpp / ComboLogicEngineSingleExe.cpp: the tracker arms and their accessors.
void OoT_ComboLogic_SetTrackerTravel(uint32_t facts);
uint32_t OoT_ComboLogic_TestTrackerTravel(int song, uint16_t* outDest);
bool OoT_ComboLogic_MMWarpTravel(int song);
void MM_ComboLogic_SetTrackerSoaring(int on);
int MM_ComboLogic_TestTrackerSoaring(void);
bool MM_ComboLogic_OoTSoaringTravel(void);
int MM_ComboLogic_TestSoaringTravelHits(int reset);
// SaveManager.cpp: OoT's frozen-age descriptor (Combo_MMWarpPlay_OoTFrozenAge).
void OoT_SlotMeta_Register(void);
}

namespace {

int sXw11Failures = 0;

#define XW11_CHECK(cond, ...)                                    \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                 \
            printf("\n");                                        \
            sXw11Failures++;                                     \
        }                                                        \
    } while (0)

const uint32_t kXw11Minuet = RSBS_XITEM2_SONG_MINUET_MM;
const uint32_t kXw11MMSongs = RSBS_XITEM2_WARP_SONG_EXTENSIONS & ~RSBS_XITEM2_SONG_SOARING_OOT;
const uint16_t kXw11Grotto = 0x0700u; // RSBS_MMWP_GROTTO_PSEUDO_FIRST: a grotto pseudo index
const uint16_t kXw11Grave = 0x031Cu;  // ENTR_REDEAD_GRAVE_0: a real entrance the logic credits

/** Freeze the shipped combo record with @p rules2 as the second word (raw: the hidden-bit refusal is Generate's). */
void Xw11FreezeWorld(uint32_t rules2) {
    ComboContext_Init();
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
}

/** A paired identity, as Combo_ForeignPairingActive reads it. */
void Xw11Pair(bool paired) {
    gComboCtx.sourceIsRando = paired;
    gComboCtx.sharedRandoSettingsHash = paired ? 0x5EEDu : 0u;
    gComboCtx.sharedRandoSeed = paired ? 0x1234u : 0u;
}

/** A frozen warp record: every song to its pad, song @p song to @p dest. */
void Xw11Record(int song, uint16_t dest) {
    uint16_t d[RSBS_MMWP_SONG_COUNT];
    for (int i = 0; i < RSBS_MMWP_SONG_COUNT; i++) {
        d[i] = Combo_MMWarpPlay_PadEntrance(i);
    }
    if (song >= 0 && song < RSBS_MMWP_SONG_COUNT) {
        d[song] = dest;
    }
    (void)Combo_OoTWarpRecord_Build(&gComboCtx.comboOoTWarps, true, false, d, NULL);
}

std::string Xw11ReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#else
    (void)rel;
    return std::string();
#endif
}

/** The body of the function starting at @p head: up to the first line that is exactly "}"; "" when absent. */
std::string Xw11Body(const std::string& text, const char* head) {
    const size_t at = text.find(head);
    if (at == std::string::npos) {
        return std::string();
    }
    const size_t end = text.find("\n}\n", at);
    return text.substr(at, end == std::string::npos ? std::string::npos : end - at);
}

int Xw11Count(const std::string& text, const char* needle) {
    int n = 0;
    const size_t len = std::strlen(needle);
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + len)) {
        n++;
    }
    return n;
}

/** The offset of the @p nth (1-based) @p needle in @p text, or npos. */
size_t Xw11Nth(const std::string& text, const char* needle, int nth) {
    size_t at = std::string::npos;
    size_t from = 0;
    for (int i = 0; i < nth; i++) {
        at = text.find(needle, from);
        if (at == std::string::npos) {
            return std::string::npos;
        }
        from = at + 1;
    }
    return at;
}

/** Are @p a < @p b < ... in order (every one found)? */
bool Xw11InOrder(const std::vector<size_t>& at) {
    for (size_t i = 0; i < at.size(); i++) {
        if (at[i] == std::string::npos || (i > 0 && at[i] <= at[i - 1])) {
            return false;
        }
    }
    return !at.empty();
}

} // namespace

// ============================================================================
// TEST BRIDGE (#1169 XW11 review, the design's T11 and its MM twin): the frozen
// world the two REAL check-tracker entry points read, for the rando-tier legs
// that drive them (leg W of games/oot/soh/oot_tracker_reachability_test.cpp and
// of games/mm/2s2h/mm_tracker_in_logic_test.cpp). Those TUs keep context.h out
// of their include set, so the combo record and the frozen halves are set here.
// Combo_Xw11TestTrackerWorldEnd puts back everything Begin and the two half
// setters change.
// ============================================================================

namespace {
ComboContext sXw11TrackerCtx;
bool sXw11TrackerBegun = false;
struct Xw11TrackerHalf {
    bool had = false;
    int live = 1;
    uint16_t returnEntrance = 0u;
    std::vector<uint8_t> bytes;
};
Xw11TrackerHalf sXw11TrackerHalves[2]; // [0] OoT, [1] MM

void Xw11TrackerTakeHalf(GameId game, Xw11TrackerHalf& half) {
    half.had = Context_HasFrozenState(game) != 0;
    half.live = Context_FrozenStateIsLiveFile(game);
    half.returnEntrance = Context_GetFrozenReturnEntrance(game);
    const size_t size = game == GAME_OOT ? (size_t)OOT_SAVE_CONTEXT_SIZE : (size_t)MM_SAVE_CONTEXT_SIZE;
    const void* blob = game == GAME_OOT ? Context_GetOoTSaveContext() : Context_GetMMSaveContext();
    half.bytes.assign(size, 0);
    if (half.had && blob != NULL) {
        memcpy(half.bytes.data(), blob, size);
    }
}

void Xw11TrackerPutHalf(GameId game, const Xw11TrackerHalf& half) {
    Context_ClearFrozenState(game);
    if (half.had) {
        Context_NoteDepartureLiveFile(game, half.live);
        Context_FreezeState(game, half.returnEntrance, half.bytes.data(), half.bytes.size());
    }
}
} // namespace

/**
 * Take the combo record and both frozen halves (once), then freeze @p rules2 as
 * the second rules word with a PRESENT warp record (every song to its pad) and
 * register both games' item adapters (their readers) and OoT's slot meta (its
 * frozen age). @p freshWorld nonzero: a fresh frozen record with a paired
 * identity (an authored MM save); zero: the created paired file's own record,
 * only its second word replaced. 1 when the frozen word reads @p rules2 and the
 * file is paired.
 */
extern "C" int Combo_Xw11TestTrackerWorldBegin(int freshWorld, uint32_t rules2) {
    if (!sXw11TrackerBegun) {
        sXw11TrackerCtx = gComboCtx;
        Xw11TrackerTakeHalf(GAME_OOT, sXw11TrackerHalves[0]);
        Xw11TrackerTakeHalf(GAME_MM, sXw11TrackerHalves[1]);
        sXw11TrackerBegun = true;
    }
    if (freshWorld != 0) {
        Xw11FreezeWorld(rules2);
        Xw11Pair(true);
    } else {
        Combo_ComboItemRules2Store(&gComboCtx.comboItemRules2, rules2);
    }
    Xw11Record(-1, 0u);
    OoT_ItemAdapter_Register();
    MM_ItemAdapter_Register();
    OoT_SlotMeta_Register();
    return (Combo_ComboCrossItemFlags2() == rules2 && Combo_ForeignPairingActive() &&
            Combo_OoTWarpRecord_Status() == (int)COMBO_OOT_WARP_REC_PRESENT)
               ? 1
               : 0;
}

/** Freeze an MM half holding the Ocarina of Time (@p ocarina) and the buttons
 *  @p buttons (MM_ItemAdapter_TestAuthorWarpSongWorld's mask), with the
 *  ownership byte @p owned. Returns OoT's facts now with OoT's crossing open
 *  (Combo_TrackerTravel_OoTFactsNow(1)), or 0xFFFFFFFF when nothing was authored. */
extern "C" uint32_t Combo_Xw11TestTrackerMMHalf(int ocarina, unsigned buttons, uint8_t owned) {
    std::vector<uint8_t> mm((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    if (MM_ItemAdapter_TestAuthorWarpSongWorld(mm.data(), mm.size(), ocarina, buttons) != 1) {
        return 0xFFFFFFFFu;
    }
    Context_ClearFrozenState(GAME_MM);
    Context_FreezeState(GAME_MM, 0, mm.data(), mm.size());
    Combo_MMWarpSongsSetOwned(owned);
    return Combo_TrackerTravel_OoTFactsNow(1);
}

/** Freeze an OoT half with the Ocarina and the Song of Soaring's buttons, owning
 *  the song when @p soaringOwned, a child when @p child. Returns MM's fact now
 *  with MM's crossing open (Combo_TrackerTravel_MMFactNow(1)), or 0xFFFFFFFF when
 *  nothing was authored. */
extern "C" uint32_t Combo_Xw11TestTrackerOoTHalf(int soaringOwned, int child) {
    std::vector<uint8_t> oot((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    if (OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), soaringOwned, 1, 1, child) != 1) {
        return 0xFFFFFFFFu;
    }
    Context_ClearFrozenState(GAME_OOT);
    Context_FreezeState(GAME_OOT, 0, oot.data(), oot.size());
    return Combo_TrackerTravel_MMFactNow(1);
}

/** Put back the combo record and both frozen halves Begin took. */
extern "C" void Combo_Xw11TestTrackerWorldEnd(void) {
    if (!sXw11TrackerBegun) {
        return;
    }
    gComboCtx = sXw11TrackerCtx;
    Xw11TrackerPutHalf(GAME_OOT, sXw11TrackerHalves[0]);
    Xw11TrackerPutHalf(GAME_MM, sXw11TrackerHalves[1]);
    sXw11TrackerBegun = false;
}

TestResult Test_WarpSongTracker(void) {
    printf("[TEST] warp-song-tracker: the added warp songs' rows and names, and the check trackers' travel facts "
           "from the frozen world (#1169 XW11)\n");
    sXw11Failures = 0;
    static ComboContext sXw11Saved; // put back at the end: this row freezes its own worlds
    sXw11Saved = gComboCtx;

    // ---- A. the row rule ----------------------------------------------------------------
    for (int i = 0; i < 7; i++) {
        const uint32_t ext = 1u << i;
        const ComboAddedSongRow off = Combo_AddedSongRow(0u, ext, true, true);
        XW11_CHECK(off.shown == 0u && off.have == 0u, "A1 bit %08X: a row with every rule off (owned)", (unsigned)ext);
        const ComboAddedSongRow on = Combo_AddedSongRow(ext, ext, false, false);
        XW11_CHECK(on.shown == 1u && on.have == 0u && on.sharedFamily == 0u, "A2 bit %08X: rule on, not owned",
                   (unsigned)ext);
        const ComboAddedSongRow held = Combo_AddedSongRow(ext, ext, true, false);
        XW11_CHECK(held.shown == 1u && held.have == 1u, "A3 bit %08X: rule on, owned", (unsigned)ext);
        const uint32_t shared = ext << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT;
        XW11_CHECK(Combo_AddedSongRow(shared, ext, true, true).shown == 0u,
                   "A4 bit %08X: the shared bit alone showed the extension's row", (unsigned)ext);
        XW11_CHECK(!Combo_AddedSongHeld(ext, ext, false, true),
                   "A5 bit %08X: the shared owner counted without the frozen shared bit", (unsigned)ext);
        XW11_CHECK(Combo_AddedSongHeld(ext | shared, ext, false, true) &&
                       Combo_AddedSongRow(ext | shared, ext, false, true).have == 1u,
                   "A6 bit %08X: the shared owner did not count under its shared bit", (unsigned)ext);
    }
    XW11_CHECK(Combo_AddedSongRow(0xFFFFFFFFu, RSBS_XITEM2_SHARED_SONG_MINUET, true, true).shown == 0u &&
                   Combo_AddedSongRow(0xFFFFFFFFu, 0x3u, true, true).shown == 0u &&
                   Combo_AddedSongRow(0xFFFFFFFFu, 0u, true, true).shown == 0u,
               "A7: a bit that is not exactly one extension showed a row");

    // ---- B. the tag ---------------------------------------------------------------------
    XW11_CHECK(std::strcmp(Combo_ForeignGameTag("Minuet of Forest (MM)", (uint8_t)GAME_MM), "") == 0 &&
                   std::strcmp(Combo_ForeignGameTag("Song of Soaring (OoT)", (uint8_t)GAME_OOT), "") == 0,
               "B1: a name ending with its own game's tag got a second one");
    XW11_CHECK(std::strcmp(Combo_ForeignGameTag("Lens of Truth", (uint8_t)GAME_MM), " (MM)") == 0 &&
                   std::strcmp(Combo_ForeignGameTag("Fairy Bow", (uint8_t)GAME_OOT), " (OoT)") == 0,
               "B2: a plain name lost its tag");
    XW11_CHECK(std::strcmp(Combo_ForeignGameTag("Song of Soaring (OoT)", (uint8_t)GAME_MM), " (MM)") == 0 &&
                   std::strcmp(Combo_ForeignGameTag("Minuet of Forest(MM)", (uint8_t)GAME_MM), " (MM)") == 0 &&
                   std::strcmp(Combo_ForeignGameTag(NULL, (uint8_t)GAME_OOT), " (OoT)") == 0 &&
                   std::strcmp(Combo_ForeignGameTag("x", (uint8_t)GAME_NONE), "") == 0,
               "B3: the other game's tag, a tag without its space, NULL or no game answered wrongly");

    // ---- C. the pure facts ----------------------------------------------------------------
    for (int song = 0; song < RSBS_TRACKER_TRAVEL_SONG_COUNT; song++) {
        const uint32_t bit = 0x02u << song;
        const uint8_t own = (uint8_t)(1u << song);
        const uint32_t play = 1u << song;
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(bit, own, play, 1, 1) == bit, "C1 song %d: every term held", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(kXw11MMSongs, 0x3Fu, 0x3Fu, 1, 1) == kXw11MMSongs,
                   "C1b: every song, every term");
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(0u, own, play, 1, 1) == 0u, "C2 song %d: the rule off", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(bit, 0u, play, 1, 1) == 0u, "C3 song %d: not owned", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(bit, own, 0u, 1, 1) == 0u, "C4 song %d: not playable", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(bit, own, play, 0, 1) == 0u, "C5 song %d: unpaired", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(bit, own, play, 1, 0) == 0u, "C6 song %d: OoT's crossing closed", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(kXw11MMSongs, own, play, 1, 1) == bit,
                   "C7 song %d: another song's bit was given", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoT(bit | (bit << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT), 0u, play, 1, 1) == 0u,
                   "C8 song %d: the shared bit alone credited a song nobody owns", song);
    }
    XW11_CHECK(Combo_TrackerTravel_IntoOoT(RSBS_XITEM2_SONG_SOARING_OOT, 0xFFu, 0xFFFFFFFFu, 1, 1) == 0u,
               "C9: OoT's own Soaring rule gave OoT a fact");
    const uint32_t soar = RSBS_XITEM2_SONG_SOARING_OOT;
    XW11_CHECK(Combo_TrackerTravel_IntoMM(soar, true, true, RSBS_MMWP_AGE_CHILD, 1, 1) == RSBS_COMBO_TRAVEL_INTO_MM,
               "C10: every term held");
    XW11_CHECK(Combo_TrackerTravel_IntoMM(0u, true, true, RSBS_MMWP_AGE_CHILD, 1, 1) == 0u &&
                   Combo_TrackerTravel_IntoMM(kXw11MMSongs, true, true, RSBS_MMWP_AGE_CHILD, 1, 1) == 0u,
               "C11: the Soaring rule off");
    XW11_CHECK(Combo_TrackerTravel_IntoMM(soar, false, true, RSBS_MMWP_AGE_CHILD, 1, 1) == 0u, "C12: not owned");
    XW11_CHECK(Combo_TrackerTravel_IntoMM(soar, true, false, RSBS_MMWP_AGE_CHILD, 1, 1) == 0u, "C13: not playable");
    XW11_CHECK(Combo_TrackerTravel_IntoMM(soar, true, true, RSBS_MMWP_AGE_ADULT, 1, 1) == 0u &&
                   Combo_TrackerTravel_IntoMM(soar, true, true, RSBS_MMWP_AGE_UNKNOWN, 1, 1) == 0u,
               "C14: an adult or unknown OoT (crossing is child-only)");
    XW11_CHECK(Combo_TrackerTravel_IntoMM(soar, true, true, RSBS_MMWP_AGE_CHILD, 0, 1) == 0u, "C15: unpaired");
    XW11_CHECK(Combo_TrackerTravel_IntoMM(soar, true, true, RSBS_MMWP_AGE_CHILD, 1, 0) == 0u,
               "C16: MM's crossing closed");
    // #1169 XW14: the shared songs' held-now bits (Combo_SharedFamilies2HeldNow) as the
    // fourth term. Each leg would fail if the Ex function ignored the bits (C17, C20),
    // honoured them without the frozen shared rule (C18, C21), or let one song's
    // shared bit stand for another (C19).
    for (int song = 0; song < RSBS_TRACKER_TRAVEL_SONG_COUNT; song++) {
        const uint32_t bit = 0x02u << song;
        const uint32_t shared = bit << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT;
        const uint32_t play = 1u << song;
        XW11_CHECK(Combo_TrackerTravel_IntoOoTEx(bit | shared, 0u, play, 1, 1, shared) == bit,
                   "C17 song %d: under its shared rule the family held now did not count as MM's copy", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoTEx(bit, 0u, play, 1, 1, shared) == 0u,
                   "C18 song %d: the family counted with the shared rule off", song);
        const uint32_t otherShared = (song == 0 ? 0x04u : 0x02u) << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT;
        XW11_CHECK(Combo_TrackerTravel_IntoOoTEx(kXw11MMSongs | (kXw11MMSongs << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT),
                                                 0u, play, 1, 1, otherShared) == 0u,
                   "C19 song %d: another song's family bit credited this song", song);
        XW11_CHECK(Combo_TrackerTravel_IntoOoTEx(bit, (uint8_t)(1u << song), play, 1, 1, 0u) ==
                       Combo_TrackerTravel_IntoOoT(bit, (uint8_t)(1u << song), play, 1, 1),
                   "C19b song %d: the Ex function with no family bits is not the plain one", song);
    }
    {
        const uint32_t soarShared = soar | RSBS_XITEM2_SHARED_SONG_SOARING;
        XW11_CHECK(Combo_TrackerTravel_IntoMMEx(soarShared, false, true, RSBS_MMWP_AGE_CHILD, 1, 1,
                                                RSBS_XITEM2_SHARED_SONG_SOARING) == RSBS_COMBO_TRAVEL_INTO_MM,
                   "C20: under the shared Soaring the family held now did not count as OoT's copy");
        XW11_CHECK(Combo_TrackerTravel_IntoMMEx(soar, false, true, RSBS_MMWP_AGE_CHILD, 1, 1,
                                                RSBS_XITEM2_SHARED_SONG_SOARING) == 0u,
                   "C21: the family counted with the shared Soaring off");
        XW11_CHECK(Combo_TrackerTravel_IntoMMEx(soarShared, false, true, RSBS_MMWP_AGE_ADULT, 1, 1,
                                                RSBS_XITEM2_SHARED_SONG_SOARING) == 0u,
                   "C22: the family made an adult OoT's Soaring a crossing");
    }

    // ---- D. the readers over each game's own layout ------------------------------------------
    std::vector<uint8_t> oot((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
    std::vector<uint8_t> mm((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    {
        uint8_t owned = 9u;
        uint8_t playable = 9u;
        XW11_CHECK(OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 1, 1, 1, 1) == 1,
                   "D0: OoT's authoring seam refused");
        XW11_CHECK(OoT_ItemAdapter_TestTrackerSoaring(oot.data(), oot.size(), &owned, &playable) == 1 && owned == 1u &&
                       playable == 1u,
                   "D1: OoT's reader over an owned, playable song read %u/%u", owned, playable);
        (void)OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 0, 1, 1, 1);
        XW11_CHECK(OoT_ItemAdapter_TestTrackerSoaring(oot.data(), oot.size(), &owned, &playable) == 1 && owned == 0u &&
                       playable == 1u,
                   "D2: OoT's reader without the song's flag");
        (void)OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 1, 0, 1, 1);
        XW11_CHECK(OoT_ItemAdapter_TestTrackerSoaring(oot.data(), oot.size(), &owned, &playable) == 1 && owned == 1u &&
                       playable == 0u,
                   "D3: OoT's reader without an ocarina");
        (void)OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 1, 1, 0, 1);
        XW11_CHECK(OoT_ItemAdapter_TestTrackerSoaring(oot.data(), oot.size(), &owned, &playable) == 1 && playable == 0u,
                   "D4: OoT's reader without the buttons");
        std::vector<uint8_t> zero((size_t)OOT_SAVE_CONTEXT_SIZE, 0);
        XW11_CHECK(OoT_ItemAdapter_TestTrackerSoaring(zero.data(), zero.size(), &owned, &playable) == 0 &&
                       owned == 0u && playable == 0u,
                   "D5: OoT's reader over an all-zero half answered");

        XW11_CHECK(MM_ItemAdapter_TestAuthorWarpSongWorld(mm.data(), mm.size(), 1, 0x1Fu) == 1,
                   "D6: MM's authoring seam refused");
        XW11_CHECK(MM_ItemAdapter_TestTrackerPlayable(mm.data(), mm.size()) == 0x3Fu,
                   "D7: MM with the Ocarina of Time and every button reads %02X, want 3F",
                   (unsigned)MM_ItemAdapter_TestTrackerPlayable(mm.data(), mm.size()));
        (void)MM_ItemAdapter_TestAuthorWarpSongWorld(mm.data(), mm.size(), 0, 0x1Fu);
        XW11_CHECK(MM_ItemAdapter_TestTrackerPlayable(mm.data(), mm.size()) == 0u, "D8: MM without the ocarina");
        // A, C-Down, C-Right: Bolero and Requiem only (can_play_tp_fire / _spirit).
        (void)MM_ItemAdapter_TestAuthorWarpSongWorld(mm.data(), mm.size(), 1, 0x07u);
        XW11_CHECK(MM_ItemAdapter_TestTrackerPlayable(mm.data(), mm.size()) == 0x0Au,
                   "D9: A, C-Down, C-Right read %02X, want 0A (Bolero, Requiem)",
                   (unsigned)MM_ItemAdapter_TestTrackerPlayable(mm.data(), mm.size()));
        // C-Right, C-Left, C-Up: Prelude only (can_play_tp_light).
        (void)MM_ItemAdapter_TestAuthorWarpSongWorld(mm.data(), mm.size(), 1, 0x1Cu);
        XW11_CHECK(MM_ItemAdapter_TestTrackerPlayable(mm.data(), mm.size()) == 0x20u,
                   "D10: C-Right, C-Left, C-Up read %02X, want 20 (Prelude)",
                   (unsigned)MM_ItemAdapter_TestTrackerPlayable(mm.data(), mm.size()));
        std::vector<uint8_t> mmZero((size_t)MM_SAVE_CONTEXT_SIZE, 0);
        XW11_CHECK(MM_ItemAdapter_TestTrackerPlayable(mmZero.data(), mmZero.size()) == 0u,
                   "D11: MM's reader over an all-zero half answered");
    }

    // ---- E. the facts now, from the frozen world ------------------------------------------
    Context_ClearAllFrozenStates();
    OoT_ItemAdapter_Register(); // the production registrations: each also registers its reader
    MM_ItemAdapter_Register();
    OoT_SlotMeta_Register();
    {
        (void)MM_ItemAdapter_TestAuthorWarpSongWorld(mm.data(), mm.size(), 1, 0x1Fu);
        Xw11FreezeWorld(kXw11Minuet);
        Xw11Pair(true);
        Combo_MMWarpSongsSetOwned(RSBS_MM_WARP_SONG_OWNED_MINUET);
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == 0u, "E1: no frozen MM half gave a fact");
        Context_FreezeState(GAME_MM, 0, mm.data(), mm.size());
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == kXw11Minuet,
                   "E2: a frozen MM half playing Minuet, owned, paired, rule on: read %08X",
                   (unsigned)Combo_TrackerTravel_OoTFactsNow(1));
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(0) == 0u, "E3: OoT's crossing closed gave a fact");
        Combo_MMWarpSongsSetOwned(0u);
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == 0u, "E4: an unowned song gave a fact");
        Combo_MMWarpSongsSetOwned(RSBS_MM_WARP_SONG_OWNED_MINUET);
        Xw11Pair(false);
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == 0u, "E5: unpaired gave a fact");
        Xw11Pair(true);
        Combo_TrackerTravel_RegisterMMReader(NULL);
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == 0u, "E6: no MM reader gave a fact");
        MM_ItemAdapter_Register();
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == kXw11Minuet,
                   "E7: MM_ItemAdapter_Register did not register MM's reader");
        Context_ClearFrozenState(GAME_MM);
        Context_NoteDepartureLiveFile(GAME_MM, 0);
        Context_FreezeState(GAME_MM, 0, mm.data(), mm.size());
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == 0u, "E8: an MM half frozen as not a live file gave a fact");
        Context_ClearFrozenState(GAME_MM);
        Context_FreezeState(GAME_MM, 0, mm.data(), mm.size());
        Xw11FreezeWorld(0u);
        Xw11Pair(true);
        Combo_MMWarpSongsSetOwned(RSBS_MM_WARP_SONG_OWNED_MINUET);
        XW11_CHECK(Combo_TrackerTravel_OoTFactsNow(1) == 0u, "E9: every rule off gave a fact");

        // MM's tracker: OoT's frozen half.
        Xw11FreezeWorld(RSBS_XITEM2_SONG_SOARING_OOT);
        Xw11Pair(true);
        XW11_CHECK(Combo_TrackerTravel_MMFactNow(1) == 0u, "E10: no frozen OoT half gave the Soaring fact");
        (void)OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 1, 1, 1, 1);
        Context_FreezeState(GAME_OOT, 0, oot.data(), oot.size());
        XW11_CHECK(Combo_TrackerTravel_MMFactNow(1) == RSBS_COMBO_TRAVEL_INTO_MM,
                   "E11: a frozen OoT child owning and playing the song, paired, rule on: read %08X",
                   (unsigned)Combo_TrackerTravel_MMFactNow(1));
        XW11_CHECK(Combo_TrackerTravel_MMFactNow(0) == 0u, "E12: MM's crossing closed gave the fact");
        Context_ClearFrozenState(GAME_OOT);
        (void)OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 1, 1, 1, 0);
        Context_FreezeState(GAME_OOT, 0, oot.data(), oot.size());
        XW11_CHECK(Combo_TrackerTravel_MMFactNow(1) == 0u, "E13: an adult OoT gave the fact");
        Context_ClearFrozenState(GAME_OOT);
        (void)OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 0, 1, 1, 1);
        Context_FreezeState(GAME_OOT, 0, oot.data(), oot.size());
        XW11_CHECK(Combo_TrackerTravel_MMFactNow(1) == 0u, "E14: an OoT that does not own the song gave the fact");
        Combo_TrackerTravel_RegisterOoTReader(NULL);
        XW11_CHECK(Combo_TrackerTravel_MMFactNow(1) == 0u, "E15: no OoT reader gave the fact");
        OoT_ItemAdapter_Register();
        Context_ClearFrozenState(GAME_OOT);
        (void)OoT_ItemAdapter_TestAuthorSoaringWorld(oot.data(), oot.size(), 1, 1, 1, 1);
        Context_FreezeState(GAME_OOT, 0, oot.data(), oot.size());
        XW11_CHECK(Combo_TrackerTravel_MMFactNow(1) == RSBS_COMBO_TRAVEL_INTO_MM,
                   "E16: OoT_ItemAdapter_Register did not register OoT's reader");

        // The destination: the frozen record's, 0 without a PRESENT one.
        Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
        XW11_CHECK(Combo_TrackerTravel_OoTDest(0) == 0u, "E17: an unset record gave a destination");
        Xw11Record(4, kXw11Grave);
        XW11_CHECK(Combo_TrackerTravel_OoTDest(4) == kXw11Grave &&
                       Combo_TrackerTravel_OoTDest(0) == Combo_MMWarpPlay_PadEntrance(0) &&
                       Combo_TrackerTravel_OoTDest(6) == 0u && Combo_TrackerTravel_OoTDest(-1) == 0u,
                   "E18: the destinations are not the record's");
    }
    Context_ClearAllFrozenStates();

    // ---- F. the engines' tracker arms ---------------------------------------------------------
    {
        Xw11Record(3, kXw11Grotto); // Requiem to a grotto pseudo index; the rest to their pads
        OoT_ComboLogic_SetTrackerTravel(kXw11MMSongs);
        uint16_t dest = 0u;
        XW11_CHECK(OoT_ComboLogic_TestTrackerTravel(0, &dest) == kXw11MMSongs &&
                       dest == Combo_MMWarpPlay_PadEntrance(0),
                   "F1: the arm or its destination is not the record's");
        XW11_CHECK(OoT_ComboLogic_MMWarpTravel(0) && OoT_ComboLogic_MMWarpTravel(5),
                   "F2: an armed song to its pad stayed closed outside a round");
        XW11_CHECK(!OoT_ComboLogic_MMWarpTravel(3),
                   "F3: Requiem to a grotto pseudo index was credited (the graph's pad would have been)");
        OoT_ComboLogic_SetTrackerTravel(kXw11Minuet);
        XW11_CHECK(OoT_ComboLogic_MMWarpTravel(0) && !OoT_ComboLogic_MMWarpTravel(1),
                   "F4: the arm opened another song's exit");
        Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
        OoT_ComboLogic_SetTrackerTravel(kXw11Minuet);
        XW11_CHECK(OoT_ComboLogic_TestTrackerTravel(0, &dest) == kXw11Minuet && dest == 0u &&
                       !OoT_ComboLogic_MMWarpTravel(0),
                   "F5: an unset record (a file from before XW7) was credited");
        Xw11Record(-1, 0u);
        OoT_ComboLogic_SetTrackerTravel(0u);
        XW11_CHECK(OoT_ComboLogic_TestTrackerTravel(0, &dest) == 0u && dest == 0u && !OoT_ComboLogic_MMWarpTravel(0),
                   "F6: disarmed, the exit stayed open or a destination stayed");
        OoT_ComboLogic_SetTrackerTravel(RSBS_XITEM2_SONG_SOARING_OOT);
        XW11_CHECK(OoT_ComboLogic_TestTrackerTravel(0, NULL) == 0u, "F7: the arm took a bit outside INTO_OOT");
        OoT_ComboLogic_SetTrackerTravel(0u);

        (void)MM_ComboLogic_TestSoaringTravelHits(1);
        XW11_CHECK(!MM_ComboLogic_OoTSoaringTravel(), "F8: MM's accessor open with nothing armed");
        MM_ComboLogic_SetTrackerSoaring(1);
        XW11_CHECK(MM_ComboLogic_TestTrackerSoaring() == 1 && MM_ComboLogic_OoTSoaringTravel(),
                   "F9: MM's armed tracker fact did not open the owl landings outside a round");
        XW11_CHECK(MM_ComboLogic_TestSoaringTravelHits(1) == 0, "F10: the tracker's read counted as a round's hit");
        MM_ComboLogic_SetTrackerSoaring(0);
        XW11_CHECK(MM_ComboLogic_TestTrackerSoaring() == 0 && !MM_ComboLogic_OoTSoaringTravel(),
                   "F11: disarmed, MM's accessor stayed open");
    }

    // ---- G. the combo pane's row text ----------------------------------------------------------
    {
        ComboTrackerCheckRow row;
        std::memset(&row, 0, sizeof(row));
        row.placedItemName = "Minuet of Forest (MM)";
        row.placedItemGame = (uint8_t)GAME_MM;
        const std::string mmInOoT = ComboGui::ComboCheckRowItemText((uint8_t)GAME_OOT, row);
        XW11_CHECK(mmInOoT == "Minuet of Forest (MM)", "G1: an MM song on an OoT check reads \"%s\"", mmInOoT.c_str());
        row.placedItemName = "Song of Soaring (OoT)";
        row.placedItemGame = (uint8_t)GAME_OOT;
        const std::string ootInMM = ComboGui::ComboCheckRowItemText((uint8_t)GAME_MM, row);
        XW11_CHECK(ootInMM == "Song of Soaring (OoT)", "G2: OoT's Soaring on an MM check reads \"%s\"",
                   ootInMM.c_str());
        row.placedItemName = "Lens of Truth";
        row.placedItemGame = (uint8_t)GAME_MM;
        const std::string lens = ComboGui::ComboCheckRowItemText((uint8_t)GAME_OOT, row);
        XW11_CHECK(lens == "Lens of Truth (MM)", "G3: an MM item on an OoT check reads \"%s\"", lens.c_str());
        row.placedItemName = "Fairy Bow";
        row.placedItemGame = (uint8_t)GAME_OOT;
        const std::string bow = ComboGui::ComboCheckRowItemText((uint8_t)GAME_MM, row);
        XW11_CHECK(bow == "Fairy Bow (OoT)", "G4: an OoT item on an MM check reads \"%s\"", bow.c_str());
        const std::string own = ComboGui::ComboCheckRowItemText((uint8_t)GAME_OOT, row);
        XW11_CHECK(own == "Fairy Bow", "G5: an item of the row's own game reads \"%s\"", own.c_str());
    }

    // ---- S. source anchors ------------------------------------------------------------------------
    {
        const char* const kO = "games/oot/soh/Enhancements/randomizer/";
        const char* const kM = "games/mm/2s2h/Rando/";
        const auto src = [](const char* dir, const char* file) {
            return Xw11ReadSource((std::string(dir) + file).c_str());
        };
        const std::string tracker = src(kO, "randomizer_check_tracker.cpp");
        const std::string hint = src(kO, "hint.cpp");
        const std::string hooks = src(kO, "hook_handlers.cpp");
        const std::string msgs = src(kO, "Messages/ItemMessages.cpp");
        const std::string pane = Xw11ReadSource("src/common/ComboTrackerWindow.cpp");
        const std::string queue = src(kM, "MiscBehavior/CheckQueue.cpp");
        const std::string mmTracker = src(kM, "CheckTracker/CheckTracker.cpp");
        const std::string gossip = src(kM, "ActorBehavior/EnGs.cpp");
        const std::string ootEngine = src(kO, "ComboLogicEngineOoT.cpp");
        const std::string mmEngine = src(kM, "ComboLogicEngineSingleExe.cpp");
        const std::string ootAdapter = src(kO, "ItemAdapterSingleExe.cpp");
        const std::string mmAdapter = src(kM, "ItemAdapterSingleExe.cpp");
        const std::string fill = src(kO, "3drando/fill.cpp");
        const std::string hints = src(kO, "3drando/hints.cpp");
        XW11_CHECK(!tracker.empty() && !hint.empty() && !hooks.empty() && !msgs.empty() && !pane.empty() &&
                       !queue.empty() && !mmTracker.empty() && !gossip.empty() && !ootEngine.empty() &&
                       !mmEngine.empty() && !ootAdapter.empty() && !mmAdapter.empty() && !fill.empty() &&
                       !hints.empty(),
                   "S0: a source file could not be read under RSBS_SOURCE_DIR");

        // The name sites: each calls the helper with the item's own game, and none appends a literal tag.
        struct Site {
            const std::string* text;
            const char* head;
            const char* call;
            int calls;
            const char* literal;
        };
        const Site sites[] = {
            { &tracker, "static std::string PlacedItemTrackerName(", "Combo_ForeignGameTag(name, GAME_MM)", 1,
              "+ \" (MM)\"" },
            { &hint, "static std::string CrossedItemHintName(", "Combo_ForeignGameTag(name, GAME_MM)", 1,
              "+ \" (MM)\"" },
            { &hooks, "extern \"C\" void OoT_Rando_Foreign_EmitPickupToast(",
              "Combo_ForeignGameTag(foreignName, GAME_MM)", 1, "+ \" (MM)\"" },
            { &msgs, "static CustomMessage BuildForeignItemText(", "Combo_ForeignGameTag(name, GAME_MM)", 1,
              "%w (MM)!" },
            { &pane, "std::string ComboCheckRowItemText(uint8_t game", "Combo_ForeignGameTag(row.placedItemName,", 1,
              "? \" (MM)\" : \" (OoT)\"" },
            { &queue, "std::string Rando::Foreign::ForeignPickupMessage(", "Combo_ForeignGameTag(name, GAME_OOT)", 1,
              "\" (OoT)!\"" },
            { &mmTracker, "static std::string ObtainedItemTrackerName(", "Combo_ForeignGameTag(foreignName, GAME_OOT)",
              1, "+ \" (OoT)\"" },
            { &mmTracker, "static std::string SeenItemTrackerName(", "Combo_ForeignGameTag(foreignName, GAME_OOT)", 1,
              "+ \" (OoT)\"" },
            { &gossip, "static std::string GossipHintItemName(const RandoSaveCheck* checks",
              "Combo_ForeignGameTag(foreignName, GAME_OOT)", 1, "+ \" (OoT)\"" },
        };
        for (const Site& s : sites) {
            const std::string body = Xw11Body(*s.text, s.head);
            XW11_CHECK(!body.empty() && Xw11Count(body, s.call) == s.calls && Xw11Count(body, s.literal) == 0,
                       "S1: %s does not call %s once, or still appends a literal tag", s.head, s.call);
        }
        XW11_CHECK(Xw11Count(Xw11Body(msgs, "static CustomMessage BuildForeignItemText("), "[[tag]]") == 2,
                   "S2: the get-item sentence does not place and replace its [[tag]]");

        // OoT's tracker: the crossing from the first search, the facts armed for a second, then cleared.
        const std::string compute = Xw11Body(
            tracker,
            "static void ComputeAvailableChecks(RandomizerRegion startingRegion, RandoAgeTime startingAgeTime) {");
        XW11_CHECK(Xw11InOrder({ Xw11Nth(compute, "ReachabilitySearch(", 1),
                                 Xw11Nth(compute, "RegionTable(RR_MARKET_MASK_SHOP)", 1),
                                 Xw11Nth(compute, "Combo_TrackerTravel_OoTFactsNow(crossOoT)", 1),
                                 Xw11Nth(compute, "OoT_ComboLogic_SetTrackerTravel(travelFacts)", 1),
                                 Xw11Nth(compute, "ReachabilitySearch(", 2),
                                 Xw11Nth(compute, "OoT_ComboLogic_SetTrackerTravel(0u)", 1),
                                 Xw11Nth(compute, "SetAvailable(true)", 1) }) &&
                       Xw11Count(compute, "ReachabilitySearch(") == 2 &&
                       Xw11Count(compute, "const int crossOoT = (maskShop != nullptr && maskShop->Child())") == 1,
                   "S3: ComputeAvailableChecks does not take OoT's crossing from its first search into a local, arm "
                   "the facts for one second search and clear them before marking checks");
        // MM's tracker: the same over its crawl.
        const std::string inLogic =
            Xw11Body(mmTracker, "std::set<RandoCheckId> ComputeChecksInLogic(s32 startEntrance, std::set");
        XW11_CHECK(Xw11InOrder({ Xw11Nth(inLogic, "CrawlReachableRegions(startEntrance)", 1),
                                 Xw11Nth(inLogic, "crawl.reachableRegions.count(RR_CLOCK_TOWER_INTERIOR)", 1),
                                 Xw11Nth(inLogic, "Combo_TrackerTravel_MMFactNow(crossMM)", 1),
                                 Xw11Nth(inLogic, "MM_ComboLogic_SetTrackerSoaring(1)", 1),
                                 Xw11Nth(inLogic, "CrawlReachableRegions(startEntrance)", 2),
                                 Xw11Nth(inLogic, "MM_ComboLogic_SetTrackerSoaring(0)", 1),
                                 Xw11Nth(inLogic, "EvaluateReachableChecks(crawl)", 1) }),
                   "S4: ComputeChecksInLogic does not take MM's crossing from its first crawl, arm the fact for one "
                   "second crawl and clear it before evaluating checks");
        // The engines read the arm outside a round only, OoT's with the record's destinations.
        const std::string ootAccess = Xw11Body(ootEngine, "extern \"C\" bool OoT_ComboLogic_MMWarpTravel(int song) {");
        const std::string ootArm = Xw11Body(ootEngine, "extern \"C\" void OoT_ComboLogic_SetTrackerTravel(");
        XW11_CHECK(Xw11Count(ootAccess, "if (!sInQuery && sOoTTrackerTravel != 0u) {") == 1 &&
                       Xw11Count(ootAccess, "Combo_MMWarpPlay_TravelEdgeOpen(true, sOoTTrackerTravel, song, "
                                            "sOoTTrackerDest[song])") == 1 &&
                       Xw11Count(ootArm, "Combo_TrackerTravel_OoTDest(song)") == 1 &&
                       Xw11Count(ootArm, "OoTComboTravelGraphDest") == 0,
                   "S5: OoT's accessor does not read the tracker arm outside a round, or the arm's destinations are "
                   "not the frozen record's");
        const std::string mmAccess = Xw11Body(mmEngine, "extern \"C\" bool MM_ComboLogic_OoTSoaringTravel(void) {");
        XW11_CHECK(Xw11Count(mmAccess, "if (!sRound.inRound && sMmTrackerSoaring) {") == 1,
                   "S6: MM's accessor does not read the tracker arm outside a round only");
        // Nothing else arms them: generation, hints and engine searches never see the facts.
        XW11_CHECK(Xw11Count(tracker, "OoT_ComboLogic_SetTrackerTravel(") == 3 &&
                       Xw11Count(fill, "SetTrackerTravel") == 0 && Xw11Count(hints, "SetTrackerTravel") == 0 &&
                       Xw11Count(ootEngine, "OoT_ComboLogic_SetTrackerTravel(") == 1 &&
                       Xw11Count(mmTracker, "MM_ComboLogic_SetTrackerSoaring(") == 3 &&
                       Xw11Count(mmEngine, "MM_ComboLogic_SetTrackerSoaring(") == 1,
                   "S7: a tracker arm is set somewhere other than the two tracker entry points");
        // Each production Register registers its reader.
        XW11_CHECK(Xw11Count(Xw11Body(ootAdapter, "extern \"C\" void OoT_ItemAdapter_Register(void) {"),
                             "Combo_TrackerTravel_RegisterOoTReader(OoTItemTrackerSoaring);") == 1 &&
                       Xw11Count(Xw11Body(mmAdapter, "extern \"C\" void MM_ItemAdapter_Register(void) {"),
                                 "Combo_TrackerTravel_RegisterMMReader(MMItemTrackerPlayable);") == 1,
                   "S8: an item adapter's Register does not register its tracker reader");
        // #1169 XW14: every player-facing reader passes the family held now, inside
        // the function that builds the row or the fact, never a literal false.
        const std::string ootRow =
            Xw11Body(ootAdapter, "ComboAddedSongRow OoTItemSoaringRow(const SaveContext* src) {");
        const std::string mmRow =
            Xw11Body(mmAdapter, "ComboAddedSongRow MMItemAddedSongRow(const SaveContext* src, int i) {");
        XW11_CHECK(Xw11Count(ootRow, "(Combo_SharedFamilies2HeldNow() & RSBS_XITEM2_SHARED_SONG_SOARING) != 0u") == 1 &&
                       Xw11Count(ootRow, ", false)") == 0,
                   "S9: OoT's Song of Soaring (OoT) row does not pass the shared family held now as its fourth "
                   "argument");
        XW11_CHECK(Xw11Count(mmRow, "const uint32_t sharedBit = def.rule << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT;") ==
                           1 &&
                       Xw11Count(mmRow, "(Combo_SharedFamilies2HeldNow() & sharedBit) != 0u") == 1 &&
                       Xw11Count(mmRow, ", false)") == 0,
                   "S10: MM's added-song row does not pass its shared family held now as its fourth argument");
        const std::string travel = Xw11ReadSource("src/common/warp_song_tracker.c");
        XW11_CHECK(Xw11Count(Xw11Body(travel, "uint32_t Combo_TrackerTravel_OoTFactsNow(int crossOoT) {"),
                             "Combo_SharedFamilies2HeldNow()") == 1 &&
                       Xw11Count(Xw11Body(travel, "uint32_t Combo_TrackerTravel_MMFactNow(int crossMM) {"),
                                 "Combo_SharedFamilies2HeldNow()") == 1 &&
                       Xw11Count(travel, "Combo_AddedSongHeld(rules2, extBit, separateOwned, false)") == 0 &&
                       Xw11Count(travel, "soaringOwned, false)") == 0,
                   "S11: a tracker travel fact does not read the shared family held now, or still passes false");
        // Each game registers its live probe in the TU that owns its save, inside
        // its family registrar's constructor (the probe is what keeps a member from
        // being asked about the other game's shadow).
        const std::string ootExports = Xw11ReadSource("games/oot/soh/GameExports_SingleExe.cpp");
        const std::string mmExports = Xw11ReadSource("games/mm/2s2h/GameExports_SingleExe.cpp");
        const auto registrar = [](const std::string& text, const char* head) {
            const size_t at = text.find(head);
            const size_t end = at == std::string::npos ? std::string::npos : text.find("\n};\n", at);
            return at == std::string::npos ? std::string() : text.substr(at, end == std::string::npos ? 0 : end - at);
        };
        XW11_CHECK(Xw11Count(registrar(ootExports, "struct OoTSharedFamilyRegistrar {"),
                             "Combo_RegisterSharedLiveProbe(GAME_OOT, OoT_SharedFamilyLiveProbe);") == 1 &&
                       Xw11Count(registrar(mmExports, "struct MMSharedFamilyRegistrar {"),
                                 "Combo_RegisterSharedLiveProbe(GAME_MM, MM_SharedFamilyLiveProbe);") == 1,
                   "S12: a game's family registrar does not register its live probe");
        XW11_CHECK(Xw11Count(Xw11Body(ootExports, "static int OoT_SharedFamilyLiveProbe(void) {"),
                             "Context_GetCurrentGame() == GAME_OOT && OoT_Combo_DepartureIsLiveFile() != 0") == 1 &&
                       Xw11Count(Xw11Body(mmExports, "static int MM_SharedFamilyLiveProbe(void) {"),
                                 "Context_GetCurrentGame() == GAME_MM && MM_Combo_DepartureIsLiveFile() != 0") == 1,
                   "S13: a live probe does not test its own game running and its own live-file gate");
        // #1169 XW14: every display site of an item's own name asks the shared
        // song's plain name first, inside the function that builds the name
        // (SharedWarpSongsMerge (10) proves the helper and MM's text name).
        const std::string mmItems = src(kM, "StaticData/Items.cpp");
        const std::string ootTrackerAdapter = src(kO, "TrackerAdapterSingleExe.cpp");
        const std::string mmTrackerAdapter = src(kM, "TrackerAdapterSingleExe.cpp");
        struct NameSite {
            const std::string* text;
            const char* head;
            const char* call;
        };
        const NameSite nameSites[] = {
            { &mmItems, "std::string GetItemName(RandoItemId randoItemId, bool includeArticle) {",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)randoItemId)" },
            { &mmTracker, "static std::string ObtainedItemTrackerName(",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)randoSaveCheck.randoItemId)" },
            { &mmTracker, "static std::string SeenItemTrackerName(",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)named)" },
            { &mmTrackerAdapter, "const char* MMTrackerPlacedItemName(const void* save",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)named)" },
            { &mmAdapter, "bool MMItemDeriveAddedSong(const SaveContext* src, int index, ComboItemRow* out) {",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_MM, (uint16_t)kMMItemAddedSongs[i].rando)" },
            { &tracker, "static std::string PlacedItemTrackerName(",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT," },
            { &ootTrackerAdapter, "const char* OoTPlacedItemName(Rando::ItemLocation* loc",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT, (uint16_t)loc->GetPlacedRandomizerGet())" },
            { &ootAdapter, "void OoTItemFillSoaring(const ComboAddedSongRow& song, ComboItemRow* out) {",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT, (uint16_t)RG_SONG_OF_SOARING_OOT)" },
            { &msgs, "void BuildCustomItemMessage(Player* player, CustomMessage& msg) {",
              "Combo_SharedFamily2DisplayName((uint8_t)GAME_OOT, (uint16_t)rgid)" },
        };
        for (const NameSite& n : nameSites) {
            const std::string body = Xw11Body(*n.text, n.head);
            XW11_CHECK(!body.empty() && Xw11Count(body, n.call) == 1,
                       "S14: %s does not ask the shared warp song's plain name once (%s)", n.head, n.call);
        }
    }

    // Put back what this row touched.
    OoT_ComboLogic_SetTrackerTravel(0u);
    MM_ComboLogic_SetTrackerSoaring(0);
    Context_ClearAllFrozenStates();
    gComboCtx = sXw11Saved;
    printf("[TEST] warp-song-tracker: %s\n", sXw11Failures == 0 ? "PASS" : "FAIL");
    return sXw11Failures == 0 ? TEST_PASS : TEST_FAIL;
}
