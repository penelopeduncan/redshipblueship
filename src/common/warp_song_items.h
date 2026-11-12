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
 * SHARED_SONG_SOARING)` and NO `addItem(OOT_SONG_SOARING)`): that family is lane
 * XW10's, and until it exists OoT's separate copy is HELD BACK, never placed.
 * Combo_SongSoaringOoTCopies is that rule, one place.
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
 * INERT UNTIL XW12. Every RSBS_XITEM2_* bit is hidden (RSBS_XITEM2_HIDDEN): Generate
 * refuses a world with one set before anything is frozen, and a load refuses one
 * another build froze. So no world a player can make has the bit, the pool never
 * gains the item, and the song pass never holds more than twelve songs.
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
 * description, with Dungeon Rewards named too, since OoT's Shuffle Songs has
 * both restricted modes, and "an Ocarina of Time song" for OoTMM's "at least one
 * song", above). Lane XW12 draws the row; the text is here so the warning ships
 * with the refusal it describes.
 */
#define RSBS_SONG_SOARING_OOT_TOOLTIP                                                                          \
    "Adds Majora's Mask's Song of Soaring to Ocarina of Time. Usable only as a child. If Shuffle Songs is on " \
    "Dungeon Rewards, you must share a song or start with an Ocarina of Time song: those twelve spots cannot " \
    "hold a thirteenth song, and Generate refuses. On Song Locations it shares the song checks of both games."

/** OoT's restricted Shuffle Songs values (RO_SONG_SHUFFLE_SONG_LOCATIONS and
 *  RO_SONG_SHUFFLE_DUNGEON_REWARDS, randomizerTypes.h), the two passes with a
 *  fixed set of spots. SongOfSoaringOoT.cpp static-asserts both. */
#define RSBS_SONG_SPOTS_SONG_LOCATIONS 1u
#define RSBS_SONG_SPOTS_DUNGEON_REWARDS 2u

/**
 * How many copies of OoT's separate Song of Soaring a world whose frozen second
 * rules word is @p rules2 puts into OoT's pool: 1 with the extension on and the
 * shared song off; 0 with the rule off, and 0 with the shared song on (the
 * family path, held back, never placed as a separate item). Pure.
 */
uint32_t Combo_SongSoaringOoTCopies(uint32_t rules2);

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

/* ---- The added songs on the trackers (#1169 lane XW11) ---------------------- */

/**
 * One added warp song's tracker row: whether a file shows it, whether the player
 * holds it, and the shared family it is a copy of.
 *   - shown: @p extBit is one of the seven extensions (RSBS_XITEM2_WARP_SONG_
 *     EXTENSIONS) and the frozen word @p rules2 has it on. A file with every rule
 *     off shows no row, so its tracker is exactly what it was before XW11.
 *   - have: shown, and Combo_AddedSongHeld.
 *   - sharedFamily: 0. Lane XW10's shared warp song returns its family slot here
 *     when the shared bit is on, so the view draws the pair once in the Shared
 *     group.
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
 * RSBS_XITEM2_WARP_SONG_SHARED_SHIFT). Lane XW10 passes its family's owned
 * query as @p sharedOwned; until then every caller passes false. Every tracker
 * row and both tracker travel facts (warp_song_tracker.h) ask this. Pure.
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

#ifdef __cplusplus
}
#endif
