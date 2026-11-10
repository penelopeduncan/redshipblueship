/**
 * @file combo_hint_view.h
 * @brief View model for the combined Hint Tracker's Majora's Mask pane: MM's
 *        hint sources, what each says, and which the player has read (#1475,
 *        lane H1; the window is lane H2's).
 *
 * WHAT PROBLEM THIS SOLVES. Majora's Mask has no hint objects: a gossip stone
 * works out what it says when it is talked to (EnGs.cpp: a stone seed from its
 * scene and position, the world's seed, a pick among the hintable checks), and
 * the fixed-location hints (the Clock Town posters, the Oath, the Hookshot, the
 * gold dust, the spider houses, the Great Fairies, the Bomb Shop's next stock)
 * build their text in their own textbox hooks. So nothing could list MM's hints,
 * say which were read, or show one while Ocarina of Time is played. This header
 * is that list, game-neutral, the way combo_tracker_view.h is the check list:
 *
 *   - THE SOURCES. Every MM hint source has a stable id (RSBS_MM_HINT_SRC_*
 *     below): the 58 gossip-stone placements in EnGs.cpp's kMmGossipStones order,
 *     then MM's 16 fixed-location hints. A stone placed in two scene versions
 *     (the Southern Swamp poisoned and cleared, Mountain Village in winter and
 *     spring) is a separate row of that table with its own scene id, so its own
 *     stone seed and its own hint: the two versions are two sources, keyed by
 *     (scene, x, z) like every other placement. Purchased hints are not sources
 *     (their pick is seeded by the frame counter).
 *   - THE RESOLVER. What a source says is a pure function of the world (the MM
 *     save's placement table, its final seed, the crossing store and the frozen
 *     Way of the Hero) and the source, computed by the MM TU (Rando/HintSources.h)
 *     with the very functions MM's textbox hooks call, so the pane's text is the
 *     textbox's text. It never touches MM's global random stream or MM's live
 *     save. One name differs on purpose: where the Bomb Shop's next-stock line
 *     names MM's own junk padding, the textbox says "Junk (<junk item>)" with a
 *     junk item drawn from that stream every 20 frames, and the pane says "Junk"
 *     (Rando/HintSources.h, JunkName).
 *   - THE READ RECORD. One bit per source in the combo save record
 *     (ComboContext.mmHintsRead, ADR 0009 claim 20), set by MM's read hook when a
 *     source's hint textbox opens. It rides every .redsave write, survives every
 *     crossing, and starts empty for a new file.
 *   - THE PANE. Rows grouped by the MM area where the hint is found, masked
 *     ("???") until read, over the same MM save the check tracker's MM pane reads
 *     (Combo_TrackerMMSave: LIVE while MM is played, STALE from the frozen MM half
 *     otherwise, UNAVAILABLE with neither), so the pane renders while OoT runs.
 *
 * ADR 0002: every MM id here (a source's hinted check, its item) is game-local
 * and travels with its GameId; nothing compares ids across games.
 *
 * Locked by mm-hint-sources (ROM-free) and mm-hint-sources-world (rando tier),
 * src/common/tests/test_mm_hint_sources.c.
 */

#ifndef RSBS_COMMON_COMBO_HINT_VIEW_H
#define RSBS_COMMON_COMBO_HINT_VIEW_H

#include "combo_area.h"         // ComboArea
#include "combo_tracker_view.h" // ComboTrackerFreshness, the MM source pick, context.h

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Majora's Mask's hint sources (stable ids: the read record's bit indices)
// ============================================================================
//
// APPEND-ONLY: an id is a bit of every .redsave's read record, so a source never
// changes its id. The stones are EnGs.cpp's kMmGossipStones rows in table order
// (that table is the ROM's own En_Gs placements and is pinned at 58 rows).

enum {
    RSBS_MM_HINT_SRC_STONE_FIRST = 0,
    RSBS_MM_HINT_SRC_STONE_COUNT = 58,
    // The Clock Town "Recruiting Soldiers" posters (EnTalk.cpp, text 0x1C06, pages 1-4).
    RSBS_MM_HINT_SRC_REMAINS_ODOLWA = 58,
    RSBS_MM_HINT_SRC_REMAINS_GOHT = 59,
    RSBS_MM_HINT_SRC_REMAINS_GYORG = 60,
    RSBS_MM_HINT_SRC_REMAINS_TWINMOLD = 61,
    // The Skull Kid on the Clock Tower rooftop, once the remains are in (DmStk.cpp, 0x2013).
    RSBS_MM_HINT_SRC_OATH = 62,
    // The Zora who overheard the pirates (EnZow.cpp, 0x12F3 .. 0x1301).
    RSBS_MM_HINT_SRC_HOOKSHOT = 63,
    // The smithy's two gold dust lines (EnKgy.cpp, 0xC49 and 0xC4B).
    RSBS_MM_HINT_SRC_GOLD_DUST = 64,
    RSBS_MM_HINT_SRC_GOLD_DUST_PRIZE = 65,
    // The spider houses' cursed men (EnSsh.cpp, 0x915; 0x1130 / 0x1131).
    RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP = 66,
    RSBS_MM_HINT_SRC_SPIDER_HOUSE_OCEAN = 67,
    // The Great Fairies (EnElfgrp.cpp, 0x578 / 0x580 and 0x582 .. 0x58C).
    RSBS_MM_HINT_SRC_FAIRY_CLOCK_TOWN = 68,
    RSBS_MM_HINT_SRC_FAIRY_WOODFALL = 69,
    RSBS_MM_HINT_SRC_FAIRY_SNOWHEAD = 70,
    RSBS_MM_HINT_SRC_FAIRY_GREAT_BAY = 71,
    RSBS_MM_HINT_SRC_FAIRY_IKANA = 72,
    // The Bomb Shop's "we should be able to sell" line (EnGirlA.cpp, 0x648).
    RSBS_MM_HINT_SRC_BOMB_SHOP_STOCK = 73,
    RSBS_MM_HINT_SRC_COUNT = 74
};

// ============================================================================
// What a source says (the resolver's answer)
// ============================================================================

/** What kind of hint a source gives. */
typedef enum {
    COMBO_HINT_TYPE_NONE = 0,    // nothing to say: no world, the hint is off, or no stone candidate
    COMBO_HINT_TYPE_ITEM = 1,    // a gossip stone: an item is at a check, in an area
    COMBO_HINT_TYPE_FOOLISH = 2, // a gossip stone: an area holds nothing important
    COMBO_HINT_TYPE_WOTH = 3,    // a gossip stone: an area is on the Way of the Hero
    COMBO_HINT_TYPE_FIXED = 4,   // a fixed-location hint: always about the same item or check(s)
} ComboHintType;

/** The textbox text's capacity, terminator included (MM's longest is the Clock Town fairy's, ~190). */
#define RSBS_HINT_TEXT_CAP 384
/** The most checks (and areas) one hint names: the Clock Town Great Fairy's two rewards. */
#define RSBS_HINT_TARGET_CAP 2

/**
 * One check a hint is about. `checkGame` is GAME_NONE when the hint names no
 * check (an area-only hint); the check id is that game's own (an OoT host for an
 * MM item that crossed). `itemGame`/`item` name what the check holds as the hint
 * presents it: the crossed item, never a crossing host's cover. `area` is the
 * check's area in the shared vocabulary, whether or not the text says it.
 */
typedef struct {
    uint8_t checkGame;
    uint16_t check;
    uint8_t itemGame;
    uint16_t item;
    ComboArea area;
} ComboHintTarget;

/**
 * A source's resolved hint. `text` is the textbox's message exactly as MM's
 * hook builds it, BEFORE colours and line breaks (MM's %r/%g/%y/%b/%p/%w colour
 * codes and its control bytes are still in it; Combo_HintPlainText strips them),
 * with the "(OoT)" marker on a crossed OoT item's name. `areas` are the areas the
 * TEXT names, in text order.
 */
typedef struct {
    uint8_t type; // ComboHintType
    uint8_t targetCount;
    ComboHintTarget targets[RSBS_HINT_TARGET_CAP];
    uint8_t areaCount;
    ComboArea areas[RSBS_HINT_TARGET_CAP];
    char text[RSBS_HINT_TEXT_CAP];
} ComboHintResolution;

/** Zero `r` (type NONE, no targets, no areas, empty text). NULL is ignored. */
void Combo_HintResolutionClear(ComboHintResolution* r);
/** Append `t` to `r`'s targets; false (nothing written) when full or NULL. */
bool Combo_HintResolutionAddTarget(ComboHintResolution* r, const ComboHintTarget* t);
/** Append `a` to the areas the text names; false when full, NULL, or `a` is no area of a game. */
bool Combo_HintResolutionAddArea(ComboHintResolution* r, ComboArea a);
/** Copy `text` into `r->text`, truncated to fit; false when it did not fit whole. */
bool Combo_HintResolutionSetText(ComboHintResolution* r, const char* text);

/**
 * `msg` as the pane prints it: MM's colour codes (%r %g %y %b %p %w) dropped,
 * every control byte (a line or box break, a sound or end code) turned into one
 * space, runs of spaces collapsed and the ends trimmed. Returns the full length
 * (snprintf's convention); `out` may be NULL with `cap` 0 to measure.
 */
int Combo_HintPlainText(const char* msg, char* out, int cap);

// ============================================================================
// Where a source is (static, per source)
// ============================================================================

typedef enum {
    COMBO_HINT_SOURCE_GOSSIP_STONE = 1,
    COMBO_HINT_SOURCE_FIXED = 2,
} ComboHintSourceKind;

typedef struct {
    uint16_t source;  // RSBS_MM_HINT_SRC_*
    uint8_t kind;     // ComboHintSourceKind
    ComboArea area;   // where the hint is read: the pane groups by it
    const char* name; // the row's label; static storage of the owning game's TU, never NULL once filled
} ComboHintSourceInfo;

// ============================================================================
// The MM adapter (registered by games/mm/2s2h/Rando/HintSourcesSingleExe.cpp)
// ============================================================================

typedef struct ComboMMHintDesc {
    uint16_t sourceCount; // RSBS_MM_HINT_SRC_COUNT
    // Static facts about `source`; false for an id past the count.
    bool (*sourceInfo)(uint16_t source, ComboHintSourceInfo* out);
    // Whether the world in the SaveContext image `save` gives this hint at all
    // (its option is on: RO_HINTS_GOSSIP_STONES for a stone, RO_HINTS_BOSS_REMAINS
    // for a poster, ...; the gold dust, fairy and Bomb Shop lines are always on).
    bool (*listed)(const void* save, uint16_t source);
    // What `source` says in the world of `save` (the live save or the frozen MM
    // half, whichever Combo_TrackerMMSave picked; MM's SaveContext layout). Pure:
    // no global random stream, no save written. False (out cleared) when the
    // image holds no randomizer world or the source says nothing there.
    bool (*resolve)(const void* save, uint16_t source, ComboHintResolution* out);
    // #1475 (lane H2), optional (NULL: every item hint is ungraded): the Journal
    // group an item `item` of game `itemGame` ranks in, SoH's ItemCategoryRank
    // order (COMBO_HINT_RANK_MAJOR .. COMBO_HINT_RANK_JUNK), or
    // COMBO_HINT_RANK_UNGRADED for an item the game cannot grade.
    int (*itemRank)(uint8_t itemGame, uint16_t item);
} ComboMMHintDesc;

/**
 * SoH's Journal item groups (randomizer_hint_tracker.cpp, ItemCategoryRank), in
 * its order: most valuable first. UNGRADED files a hint under the plain "Items"
 * group instead (SoH's kItemSlotUngraded).
 */
enum {
    COMBO_HINT_RANK_MAJOR = 0,
    COMBO_HINT_RANK_BOSS_KEY = 1,
    COMBO_HINT_RANK_SMALL_KEY = 2,
    COMBO_HINT_RANK_SKULLTULA_TOKEN = 3,
    COMBO_HINT_RANK_HEALTH = 4,
    COMBO_HINT_RANK_LESSER = 5,
    COMBO_HINT_RANK_JUNK = 6,
    COMBO_HINT_RANK_UNGRADED = 7,
};

/** Install MM's descriptor (copied). NULL un-registers. A descriptor with a NULL
 *  member (itemRank excepted) or a source count past the read record's capacity is refused. */
void Combo_Hint_RegisterMM(const ComboMMHintDesc* desc);
/** The registered MM descriptor, or NULL. */
const ComboMMHintDesc* Combo_Hint_GetMMDesc(void);

/**
 * Build and register MM's descriptor. DEFINED MM-SIDE
 * (games/mm/2s2h/Rando/HintSourcesSingleExe.cpp); MM_TrackerAdapter_Register
 * calls it, so every path that brings up the combo tracker's MM adapter
 * (Combo_TrackerWindow_Init, which main.cpp calls) brings this one up too.
 */
void MM_HintAdapter_Register(void);

// ============================================================================
// The read record (ComboContext.mmHintsRead; ADR 0009 claim 20)
// ============================================================================

/** Whether `game`'s source `source` has been read. Only GAME_MM has a record (OoT's is SoH's, lane H2). */
bool Combo_HintIsRead(uint8_t game, uint16_t source);
/** Record that `game`'s source `source` was read; true when this call set it. Only GAME_MM. */
bool Combo_HintMarkRead(uint8_t game, uint16_t source);
/** How many of `game`'s sources are read. */
int Combo_HintReadCount(uint8_t game);

// ============================================================================
// The pane's reads (all null-safe; only GAME_MM answers in lane H1)
// ============================================================================

typedef struct {
    uint8_t freshness; // ComboTrackerFreshness of the MM save the pane reads
    bool hasWorld;     // that save holds a randomizer world
    int sourceCount;   // rows the pane lists (the world's enabled sources)
    int readCount;     // of those, read
} ComboHintGameSummary;

/** Fill `out` for `game`; UNAVAILABLE zeros when there is nothing to read. NULL `out` is ignored. */
void Combo_HintGameSummary(uint8_t game, ComboHintGameSummary* out);

/**
 * One row of the pane. `known` is whether `hint` holds the source's resolved
 * hint: only once the source is read and the world is readable, so an unread
 * row carries nothing the player has not seen. `display` is what the row
 * prints: "???" until known, then Combo_HintPlainText of the hint's text.
 * `areaName` is Combo_Area_Name(info.area), the group header. `found` (known
 * rows only) is SoH's Journal "found": the hint names at least one check and
 * every check it names is collected in the save the pane reads; a hint naming
 * another game's check is never found here (that game's state is lane H2's).
 */
typedef struct {
    ComboHintSourceInfo info;
    const char* areaName;
    bool read;
    bool known;
    bool found;
    ComboHintResolution hint;
    char display[RSBS_HINT_TEXT_CAP];
} ComboHintRow;

/** The text an unread row prints. */
#define RSBS_HINT_MASKED_TEXT "???"

/** Rows `game`'s pane lists; 0 when UNAVAILABLE or the save holds no randomizer world. */
int Combo_HintRowCount(uint8_t game);

/**
 * Row `index`, in the pane's order: grouped by area (ascending area id of the
 * source's game's half), then by source id. False out of range / unavailable.
 */
bool Combo_HintRowAt(uint8_t game, int index, ComboHintRow* out);

/**
 * Every row Combo_HintRowAt answers, in its order, into `out` (`cap` entries),
 * with the order worked out once (the window's per-frame read; RowAt alone works
 * it out per call). Returns how many rows there are, which may exceed `cap`.
 */
int Combo_HintRowsAll(uint8_t game, ComboHintRow* out, int cap);

// ============================================================================
// The combined window's OoT pane (lane H2): the OoT adapter over SoH's Hint
// Tracker data (#1468, randomizer_hint_tracker.cpp)
// ============================================================================

/** What a pane has to list: SoH's Hint Tracker's three states (HintTrackerWindow::DrawElement). */
typedef enum {
    COMBO_HINT_PANE_WAITING = 0,   // no file to read: SoH's "Waiting for file load..."
    COMBO_HINT_PANE_NOT_RANDO = 1, // a file with no randomizer world: SoH's rando-only note
    COMBO_HINT_PANE_READY = 2,     // a randomizer world: its rows follow (none: SoH's "No hints" note)
} ComboHintPaneState;

/** The colour a Journal entry's name takes: SoH's Way of the Hero and Foolish colours, or none. */
typedef enum {
    COMBO_HINT_COLOUR_NONE = 0,
    COMBO_HINT_COLOUR_WOTH = 1,
    COMBO_HINT_COLOUR_FOOLISH = 2,
} ComboHintNameColour;

/** The Locations group key of a hint read nowhere in particular: SoH's "Other Hints", drawn last. */
#define RSBS_HINT_GROUP_OTHER 0xFFFFFFFFu

/**
 * One hint as the combined window lists it, in both of SoH's views, with every
 * decision SoH's DrawHintList makes for it already made by the game that owns
 * it. Strings are the owning adapter's, valid until its next collect.
 *
 * Locations view: the entry `name` (and, read, `text` below it, else "???")
 * under group `locationKey` / `locationName`, groups in ascending key order.
 * `locationIsArea`: the key is an area key the game's current area
 * (Combo_TrackerCurrentArea) can equal, for SoH's scroll to the current area.
 *
 * Journal view (read hints only): under group `journalKey` / `journalName`; the
 * entry's name is `journalLabel` (NULL: `name`), drawn alone when
 * `journalCompact` (a Way of the Hero or Foolish hint's area names), in
 * `journalColour`; entries sort found last, then by `sortRank`, then by name.
 * `found`: every check the hint names is collected (SoH's tick and dim).
 */
typedef struct {
    uint16_t source; // the game-local hint id (OoT: RandomizerHint; MM: RSBS_MM_HINT_SRC_*)
    bool read;
    const char* name;
    const char* text; // read rows: the hint's text, its messages one per line; NULL unread
    uint32_t locationKey;
    const char* locationName;
    bool locationIsArea;
    uint32_t journalKey;
    const char* journalName;
    const char* journalLabel;
    bool journalCompact;
    uint8_t journalColour; // ComboHintNameColour
    int sortRank;
    bool found;
} ComboHintPaneRow;

typedef struct ComboOoTHintOps {
    // Gather OoT's rows now (SoH's DrawHintList's walk: the gossip stones, then
    // every other enabled hint); *outCount is how many (0 unless READY). Returns
    // a ComboHintPaneState. Live while OoT plays; from the suspended heap (the
    // world OoT departed from while playing a randomizer file) otherwise.
    uint8_t (*collect)(int* outCount);
    // Row `index` of the last collect; false out of range.
    bool (*rowAt)(int index, ComboHintPaneRow* out);
    // Mark OoT check `checkId` Seen for a hint Majora's Mask showed that names it
    // (an MM item that crossed to that OoT host): SoH's RCSHOW_SEEN_OR_HINTED, on
    // the suspended heap while MM plays the paired file. False with nothing
    // written otherwise, or when the check is past unchecked. May itself be NULL.
    bool (*markHostSeen)(uint16_t checkId);
} ComboOoTHintOps;

/** Install OoT's ops (kept by pointer: static storage). NULL un-registers; ops missing collect or rowAt are refused. */
void Combo_Hint_RegisterOoT(const ComboOoTHintOps* ops);
/** The registered OoT ops, or NULL. */
const ComboOoTHintOps* Combo_Hint_GetOoTOps(void);

/**
 * Build and register OoT's ops. DEFINED OoT-SIDE
 * (games/oot/soh/Enhancements/randomizer/randomizer_hint_tracker.cpp);
 * Combo_TrackerWindow_Init calls it with the check tracker's adapters.
 */
void OoT_HintAdapter_Register(void);

// ============================================================================
// Cross-game Seen (lane H2): a hint that names a check in the other game
// ============================================================================
//
// A hint names a check of the other game when the item it names crossed: an MM
// hint about an MM item now at an OoT host. The host is marked Seen in its own
// game, as a hint about a check of that game marks it (SoH's
// ApplyItemHintToChecks; MM's MarkCheckSeen). No OoT hint names an MM check:
// OoT's hints that find an item somewhere name a Termina slot by its MM AREA
// only (HINT_TYPE_AREA, hint.cpp's foreignAreas), and SoH marks no check Seen
// for an area hint, so that direction has nothing to mark.

/** The most host checks Combo_HintReadHostChecks can name: every read MM source's every target. */
#define RSBS_HINT_HOST_CAP (RSBS_MM_HINT_READ_CAP * RSBS_HINT_TARGET_CAP)

/**
 * Mark `hostGame`'s check `check` Seen for a hint the other game is showing now
 * (MM's read hook, Rando::HintSources::MarkRead). Only GAME_OOT has a host to
 * mark (see above), through the OoT ops' markHostSeen. True when it was marked.
 */
bool Combo_HintMarkHostSeen(uint8_t hostGame, uint16_t check);

/**
 * The checks of `hostGame` that the other game's READ hints name, each once,
 * into `out` (`cap` entries); returns how many there are. For GAME_OOT: every
 * read MM source's targets on an OoT check, resolved over the MM save the pane
 * reads, but only while that save is MM's live one or the armed frozen half
 * (Context_HasFrozenState), never a stale image another file left. OoT's load
 * seam re-applies them (OoT_TrackerAdapter_ApplyMMHintsSeen), because a Seen
 * written to the suspended heap reaches OoT's .sav only at OoT's next save.
 * 0 for GAME_MM (nothing names one) or with nothing to read.
 */
int Combo_HintReadHostChecks(uint8_t hostGame, uint16_t* out, int cap);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_HINT_VIEW_H
