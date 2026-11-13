#pragma once
/**
 * @file warp_song_items.h
 * @brief The cross-game warp songs' item rules that are neither game's (#1169,
 *        lane XW4; OoTMM songSoaringOot and sharedSongSoaring). Game-header-free.
 *
 * THE SONG OF SOARING (OoT). Under the frozen second-word rule
 * RSBS_XITEM2_SONG_SOARING_OOT (foreign_items.h) Ocarina of Time's pool gains one
 * separate item, "Song of Soaring (OoT)" (RG_SONG_OF_SOARING_OOT,
 * games/oot/soh/Enhancements/randomizer/SongOfSoaringOoT.cpp). Under the shared
 * rule RSBS_XITEM2_SHARED_SONG_SOARING (which requires the extension) OoTMM
 * instead replaces Majora's Mask's copy with one shared item
 * (logic/src/world/transform.ts: `replaceItem(MM_SONG_SOARING,
 * SHARED_SONG_SOARING)` and NO `addItem(OOT_SONG_SOARING)`). OoTMM adds no copy
 * under the shared rule, so OoT pools none: Majora's Mask's native copy is the
 * shared row, and the two songs are the family's two members (lane XW10,
 * warp_song_family.h). Combo_SongSoaringOoTCopies is that rule, one place.
 *
 * ONE SPOT SHORT. OoT has twelve song spots under Shuffle Songs: Song Locations
 * (ShuffleSongs.cpp) and twelve under Dungeon Rewards (the eight boss hearts, the
 * Bottom of the Well lens chest, the Gerudo Training Ground final chest, Sheik in
 * Ice Cavern and Impa; 3drando/fill.cpp), and twelve songs of its own. A separate
 * Song of Soaring makes thirteen, so OoTMM refuses before placing anything
 * (solver/solve.ts, placeSemiShuffled: "Not enough locations for
 * songs/dungeon rewards"). Operator ruling 2026-11-04 (decision 2): OoTMM's
 * behaviour exactly, so Generate refuses with that wording, the option's text
 * warns, and the option is never greyed out. Combo_SongSpotsOverflow is the count
 * and the wording; OoT's restricted song pass asks it after the starting songs
 * left the pool and after the #1085 shared hold-back, the two ways a player makes
 * room (OoTMM's "share or start with at least one song").
 *
 * SINCE #1542 S3 THE LIMIT IS DUNGEON REWARDS' ALONE. Under Song Locations the song
 * checks of both games are one pool (the shared song pool, rulings D1 and D5, as in
 * OoTMM's songLocations), placed by the single bag, so the thirteen OoT songs share
 * the 23 song checks with MM's ten and generate; that pool's own capacity refusal
 * (Combo_SongPoolCapacityRefusal, combo_song_pool.h) begins with the same sentence.
 * Under Dungeon Rewards OoT still fills its own twelve spots, so only an Ocarina of
 * Time starting song frees one of them; the texts therefore say "start with an
 * Ocarina of Time song", a stated divergence from OoTMM's wording.
 *
 * OFFERED SINCE XW12. The rule has a Cross-Game Songs row (off by default). Under
 * Song Locations a paired world always arms the shared song pool, so the Song
 * Locations branch of Combo_SongSpotsOverflow is a BACKSTOP no production world
 * reaches (Generate refuses an unpaired world, #657): it runs only with the song
 * hosts forced off for a test (Combo_SongHostsForceForTest(0)), and it begins with
 * the union's own sentence, so the two cannot disagree. The Dungeon Rewards
 * branch is reachable, and its refusal reaches the player (Combo_GenRefusal_*).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** OoTMM's refusal, verbatim (packages/logic/src/solver/solve.ts,
 *  placeSemiShuffled). Every refusal message begins with it. */
#define RSBS_SONG_SPOTS_REFUSAL "Not enough locations for songs/dungeon rewards"

/**
 * The Song of Soaring (OoT) row's tooltip (OoTMM data.ts songSoaringOot's
 * description in the page's own verb-first form, with Dungeon Rewards named too,
 * since OoT's Shuffle Songs has both restricted modes, and "an Ocarina of Time
 * song" for Dungeon Rewards, above). Under Song Locations the added song is one
 * more row of the shared song pool (23 checks for 22 songs with the defaults),
 * so with Crossing Direction Both one added song fits and a second needs a
 * shared or starting song. When Ocarina of Time's songs stay home (Crossing
 * Direction Off or MM Items to OoT: Combo_ComboDirectionArms(GAME_OOT) false,
 * so the single bag confines them, RSBS_SONG_POOL_SHORT_OOT) the Song of Soaring
 * (OoT) alone already makes 13 Ocarina of Time songs for OoT's 12 song checks,
 * and only an Ocarina of Time song shared or started with frees one
 * (SongPoolOverCapacity leg (b)); the text says so. The way out names only
 * remedies this build offers (Shuffle Songs: Anywhere, or a starting song):
 * the Shared rows stay hidden until XW10, which restores OoTMM's "share a
 * song". The Cross-Game Songs row draws it (SohMenuCombo.cpp).
 */
#define RSBS_SONG_SOARING_OOT_TOOLTIP                                                                            \
    "Adds Majora's Mask's Song of Soaring to Ocarina of Time, usable only as a child. On Shuffle Songs: Song "   \
    "Locations or Dungeon Rewards, Generate refuses when the songs outnumber the song checks they can use: set " \
    "Shuffle Songs to Anywhere or start with a song (an Ocarina of Time song, for Dungeon Rewards or when "      \
    "Ocarina of Time items stay in Ocarina of Time)."

/** OoT's restricted Shuffle Songs values (RO_SONG_SHUFFLE_SONG_LOCATIONS and
 *  RO_SONG_SHUFFLE_DUNGEON_REWARDS, randomizerTypes.h), the two passes with a
 *  fixed set of spots. SongOfSoaringOoT.cpp static-asserts both. */
#define RSBS_SONG_SPOTS_SONG_LOCATIONS 1u
#define RSBS_SONG_SPOTS_DUNGEON_REWARDS 2u
/** OoT's Shuffle Songs: Off (RO_SONG_SHUFFLE_OFF): every song at its vanilla
 *  check, no song pass. SongOfSoaringOoT.cpp static-asserts it. */
#define RSBS_SONG_SPOTS_OFF 0u

/**
 * How many copies of OoT's separate Song of Soaring a world whose frozen second
 * rules word is @p rules2 puts into OoT's pool: 1 with the extension on and the
 * shared song off; 0 with the rule off, and 0 with the shared song on (OoTMM adds
 * no copy under the shared rule; MM's native copy is the shared row). Pure.
 */
uint32_t Combo_SongSoaringOoTCopies(uint32_t rules2);

/**
 * THE SONG OF SOARING (OoT)'s DOMAIN UNDER Shuffle Songs: Off (#1169 XW12, D5).
 * Off places every OoT song at its vanilla check and the separate Song of
 * Soaring (OoT) has none, so it waits in the pool for the general pass, the
 * single bag. Before XW12 it could leave Ocarina of Time only when an unrelated
 * shared song was held back (the #1085 arm), so one setting moved another item's
 * domain. Now the song's own rule decides: true under Off (@p shuffleSongs is
 * RSBS_SONG_SPOTS_OFF) whenever OoT pools a copy (Combo_SongSoaringOoTCopies of
 * @p rules2), and OoT's fill arms its songs to roam, as OoTMM places an added
 * song anywhere when songs are not on a restricted pass. Under Off no other song
 * is in the pool, so nothing else moves; with the rule off it is false, so no
 * rules-off world changes. Pure.
 */
bool Combo_SongSoaringOoTRoamsUnderOff(uint32_t shuffleSongs, uint32_t rules2);

/**
 * THE OVERFLOW REFUSAL. @p songs songs for @p spots spots in the restricted song
 * pass @p mode (RSBS_SONG_SPOTS_*): returns how many songs do not fit (0 when
 * every song has a spot). When it is not 0 and @p msg is not NULL, writes the
 * refusal, beginning with RSBS_SONG_SPOTS_REFUSAL, naming the counts and the
 * mode, and, when @p soaringOoT (the separate Song of Soaring is among the
 * songs), naming it and the ways out. Always NUL-terminates @p msg when @p len >
 * 0 ("" when nothing overflows). Pure.
 */
uint32_t Combo_SongSpotsOverflow(uint32_t songs, uint32_t spots, uint32_t mode, bool soaringOoT, char* msg, size_t len);

/** Record the refusal a generation ended with (NULL or "" clears it). OoT's song
 *  pass records it as it refuses; the locks read it back. */
void Combo_SongSpotsRecordRefusal(const char* msg);

/** The last recorded refusal, or "" when none. Never NULL. */
const char* Combo_SongSpotsLastRefusal(void);

/* ---- Why the OoT generation refused, for the player (#1169 XW12, D4) ------
 *
 * Two refusals are taken on the generation worker, inside Playthrough_Init,
 * before any paired world exists: OoT's own song pass under Shuffle Songs:
 * Dungeon Rewards (Combo_SongSpotsOverflow) and the hidden-rules gate (a shared
 * warp-song key set with its extension). Before XW12 both were logged only. The
 * worker NOTES which one ended the generation; the game thread TAKES the note
 * once, after the worker finished (OoT_SeedPackage_FileSelectTick), and raises
 * "Not generated:" with Combo_GenRefusal_Line. The worker's last store happens
 * before the release of its finished flag and the take after its acquire, so a
 * plain int is enough. */
#define RSBS_GEN_REFUSAL_NONE 0
#define RSBS_GEN_REFUSAL_SONG_SPOTS 1  // OoT's own song pass could not hold the songs
#define RSBS_GEN_REFUSAL_HIDDEN_RULE 2 // a rule this build does not offer yet

/** Note why the generation refused (RSBS_GEN_REFUSAL_NONE clears it). */
void Combo_GenRefusal_Note(int kind);
/** The note, cleared as it is read. */
int Combo_GenRefusal_Take(void);
/** The toast's line after "Not generated:" for @p kind, or NULL for NONE and any
 *  unknown kind. Each fits the overlay's one line after the prefix. Pure. */
const char* Combo_GenRefusal_Line(int kind);

/* ---- The added songs on the trackers (#1169 lane XW11) ---------------------- */

/**
 * One added warp song's tracker row: whether a file shows it, whether the player
 * holds it, and the shared family it is a copy of.
 *   - shown: @p extBit is one of the seven extensions (RSBS_XITEM2_WARP_SONG_
 *     EXTENSIONS) and the frozen word @p rules2 has it on. A file with every rule
 *     off shows no row, so its tracker is exactly what it was before XW11.
 *   - have: shown, and Combo_AddedSongHeld.
 *   - sharedFamily: always 0, under the shared rule too (#1169 XW14). The item
 *     view's Shared group reads the FIRST family word only (combo_item_view.c,
 *     SharedFamiliesDrawnShared and RowLeftOut), and a second-word family's bit
 *     would collide with a first-word slot there. So each game keeps its own
 *     row, and under the shared rule both show the song held (OoTMM shows the
 *     song in both games' menus).
 */
typedef struct ComboAddedSongRow {
    uint8_t shown;
    uint8_t have;
    uint32_t sharedFamily;
} ComboAddedSongRow;

/**
 * THE ONE OWNERSHIP QUESTION for an added warp song ("does the player hold song
 * @p extBit in the game it was added to"): @p separateOwned (the separate copy's
 * own record: OoT's RAND_INF_OBTAINED_SONG_OF_SOARING_OOT, MM's
 * gComboCtx.mmWarpSongsOwned bit), or @p sharedOwned when the frozen word
 * @p rules2 has the shared song over @p extBit on (extBit <<
 * RSBS_XITEM2_WARP_SONG_SHARED_SHIFT). Every caller passes the family's
 * held-now answer as @p sharedOwned (Combo_SharedFamilies2HeldNow, #1169 XW14).
 * Every tracker row and both tracker travel facts (warp_song_tracker.h) ask
 * this. Pure.
 */
bool Combo_AddedSongHeld(uint32_t rules2, uint32_t extBit, bool separateOwned, bool sharedOwned);

/** The row for extension @p extBit under frozen word @p rules2 (above). Pure. */
ComboAddedSongRow Combo_AddedSongRow(uint32_t rules2, uint32_t extBit, bool separateOwned, bool sharedOwned);

/* ---- The other game's tag after an item's name (#1169 lane XW11) ------------ */

/**
 * The tag a crossed item's name takes after it, for an item of @p originGame
 * (a GameId: 1 OoT, 2 MM): " (OoT)" or " (MM)", or "" when @p name already ends
 * with exactly that tag. The seven added warp songs are named with their own
 * game's tag (OoTMM's option names, "Minuet of Forest (MM)", "Song of Soaring
 * (OoT)"), so every site that appends the origin's tag printed it twice; the
 * same holds for #980's "Gerudo Mask (MM)" and its siblings. Any other
 * @p originGame answers "". NULL @p name answers the tag. Never NULL. Pure.
 */
const char* Combo_ForeignGameTag(const char* name, uint8_t originGame);

/* ---- A shared warp song's name (#1169 lane XW14) ----------------------------- */

/**
 * The name a shared warp song's copy is shown by, with no game tag: under its
 * frozen shared rule the song is ONE item of both games, and OoTMM names it
 * without "(OoT)" / "(MM)" (generator text.c isItemAmbiguous: a song is ambiguous,
 * so tagged, only while `!CFG_SHARED_SONG_X && CFG_<ext>`). Returns "Song of
 * Soaring" or the Ocarina of Time song's own name (Combo_MMWarpSongOoTName,
 * "Minuet of Forest" ...) when @p nativeId is @p game's (a GameId: 1 OoT, 2 MM)
 * REGISTERED member of a second-word family the frozen word ARMS
 * (shared_resources.h); NULL for anything else, so a caller keeps its own name
 * and tag. A display name only: never a lookup key (the item tables' names and
 * the foreign-name lookups stay what they are). A site that shows the result
 * appends no game tag after it. Never touches a member's callbacks.
 */
const char* Combo_SharedFamily2DisplayName(uint8_t game, uint16_t nativeId);

#ifdef __cplusplus
}
#endif
