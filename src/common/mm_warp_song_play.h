#pragma once
/**
 * @file mm_warp_song_play.h
 * @brief Ocarina of Time's six warp songs PLAYED in Majora's Mask (#1169 lane
 *        XW7; OoTMM packages/generator/src/mm/ocarina.c and
 *        packages/generator/src/mm/actors/En_Test7.c): the note matcher, the
 *        prompt's gates and text, the En_Test7 departure's params, the pad table,
 *        OoT's frozen age and the creation's warp-song record. Game-header-free.
 *
 * THE MATCHER. OoTMM never widens MM's song masks (all 24 song ids are used, and
 * AudioOcarina_Start keeps `flags & 0xFFFFFF`). It compares the tail of the
 * free-play note buffer with each owned custom song (common/ocarina.c,
 * comboCheckSong) and, on a match, publishes the staff state 0x80 + i.
 * Combo_MMWarpPlay_TailMatches is that loop over MM's note values; MM's call site
 * is AudioOcarina_CheckSongsWithoutMusicStaff (games/mm/src/audio/
 * code_8019AF00.c), AFTER the vanilla loop and only when no vanilla song matched
 * on the same note, so a vanilla song completed on that note wins (OoTMM's tie:
 * its vanilla loop runs after the custom check and overwrites it).
 *
 * THE GATES (operator rulings of 2026-11-04 on #1169; OoTMM
 * Ocarina_HandleLastPlayedSong). A song MATCHES only under its frozen rule, owned,
 * in a paired world, in free play. Once played, Combo_MMWarpPlay_Decide asks, in
 * order: can Ocarina of Time be run now (pairing, the switch target, nothing
 * pending); does the scene restrict the Song of Soaring (any nonzero
 * restriction); does the file record its warp songs (the record below); is OoT's
 * frozen half a child (crossing is child-only); can the destination be resolved.
 * Every refusal shows MM's own 0x1B95 and ends the ocarina.
 *
 * THE RECORD (ComboOoTWarpRecord, context.h, ADR 0009 claim 22). What the prompt
 * names and where the arrival lands are the file's, frozen at creation: nothing
 * here reads Ocarina of Time's heap. MM always arms the song's VANILLA pad;
 * OoT's arrival resolves it through the record (Entrance_OverrideWarpArrival,
 * randomizer_entrance.c).
 *
 * OFFERED SINCE XW13. The six rules have Cross-Game Songs rows, drawn since the
 * integration row IntMMWarpSongMinuet played the Minuet in Majora's Mask and
 * arrived in Ocarina of Time; with every one off (the default) no song matches,
 * nothing is armed, and this module changes nothing a player can see or hear.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "context.h" // ComboOoTWarpRecord

#ifdef __cplusplus
extern "C" {
#endif

/** The six songs, OoTMM's order (Minuet, Bolero, Serenade, Requiem, Nocturne,
 *  Prelude), which is the order of the XW5 rule and ownership bits. */
#define RSBS_MMWP_SONG_COUNT 6
/** The staff states the matcher publishes: 0x80 + song (OoTMM). Above every
 *  vanilla MM state (0..23, 0xFE, 0xFF never reach it). */
#define RSBS_MMWP_STATE_FIRST 0x80u
#define RSBS_MMWP_STATE_LAST 0x85u
/** The XW2 rule bits of the six MM extensions (RSBS_XITEM2_SONG_MINUET_MM ..
 *  RSBS_XITEM2_SONG_PRELUDE_MM; mm_warp_song_play.c static-asserts it). */
#define RSBS_MMWP_RULE_MASK 0x0000007Eu
/** En_Test7's params for a warp song (OoTMM: 0x8000 | song). As an s16 it is
 *  negative: never ENTEST7_ARRIVE (-1) and never an owl mode (0x1C..0x26). */
#define RSBS_MMWP_ENTEST7_FLAG 0x8000u
#define RSBS_MMWP_ENTEST7_PARAMS(song) ((uint16_t)(RSBS_MMWP_ENTEST7_FLAG | (uint16_t)(song)))
/** The first grotto pseudo index (SoH's ENTRANCE_GROTTO_LOAD_START; SaveManager.cpp
 *  static-asserts it). OoT turns one into a real entrance only through the seed's
 *  grotto tables. */
#define RSBS_MMWP_GROTTO_PSEUDO_FIRST 0x0700u
/** MM's own text: "You can't use that song here" (z_message.c's restricted-song
 *  refusal, every refusal of this lane), and the Soaring prompt's box (0x1B93),
 *  whose header the warp prompt borrows. */
#define RSBS_MMWP_TEXT_RESTRICTED 0x1B95u
#define RSBS_MMWP_TEXT_PROMPT_BOX 0x1B93u
#define RSBS_MMWP_TEXT_PLAYED_BOX 0x1B78u

/* ---- The songs ------------------------------------------------------------ */

/** OoTMM's buttons for @p song (sCustomSongs, mm/ocarina.c) as MM's OCARINA_BTN
 *  indices (A 0, C-down 1, C-right 2, C-left 3, C-up 4); *outCount gets the
 *  length. NULL (and 0) out of range. */
const uint8_t* Combo_MMWarpPlay_Buttons(int song, uint8_t* outCount);
/** The song's name ("Minuet of Forest" ...), NULL out of range. */
const char* Combo_MMWarpPlay_SongName(int song);

/**
 * OoTMM's comboCheckSong: whether the last @p n notes of @p buf (whose next free
 * position is @p appendPos, so the notes are buf[0..appendPos-1]) equal
 * @p buttons mapped through @p btnToPitch (MM's sButtonToPitchMap, 5 entries).
 * False for a NULL argument, n == 0, fewer than n notes or a button index >= 5.
 */
bool Combo_MMWarpPlay_TailMatches(const uint8_t* buf, uint8_t appendPos, const uint8_t* buttons, uint8_t n,
                                  const uint8_t* btnToPitch);
/** The first song in @p usableMask (bit i = song i) whose buttons end @p buf
 *  (Combo_MMWarpPlay_TailMatches), or -1. */
int Combo_MMWarpPlay_MatchSong(const uint8_t* buf, uint8_t appendPos, const uint8_t* btnToPitch, uint8_t usableMask);
/**
 * Whether a Termina wall of @p n staff buttons lets a song of @p usableMask
 * complete BEFORE the wall's last note (a window ending at note 1..n-1): played
 * note by note, that song would steal the wall. A window ending on the last note
 * ties, and the vanilla wall wins a tie. An addition with no OoTMM counterpart.
 */
bool Combo_MMWarpPlay_WallStealsBeforeEnd(const uint8_t* buttons, uint8_t n, uint8_t usableMask);

/** Whether @p state is one of the six staff states (0x80..0x85). */
bool Combo_MMWarpPlay_IsStaffState(unsigned state);
/** The staff state of @p song, 0 out of range. */
uint8_t Combo_MMWarpPlay_StaffState(int song);
/** The song of staff state @p state, -1 for any other state. */
int Combo_MMWarpPlay_SongOfStaffState(unsigned state);

/** Whether En_Test7's @p params (an s16) are a warp song's (0x8000..0x8005).
 *  False for ENTEST7_ARRIVE, every owl mode and anything else. */
bool Combo_MMWarpPlay_IsEnTest7Params(int16_t params);
/** The song of En_Test7 @p params, -1 unless Combo_MMWarpPlay_IsEnTest7Params. */
int Combo_MMWarpPlay_SongOfEnTest7Params(int16_t params);

/* ---- The pads --------------------------------------------------------------- */

/** OoT's vanilla warp pad of @p song (ENTR_SACRED_FOREST_MEADOW_WARP_PAD 0x0600,
 *  DEATH_MOUNTAIN_CRATER 0x04F6, LAKE_HYLIA 0x0604, DESERT_COLOSSUS 0x01F1,
 *  GRAVEYARD 0x0568, TEMPLE_OF_TIME 0x05F4; SaveManager.cpp static-asserts them),
 *  what MM arms; 0 out of range. */
uint16_t Combo_MMWarpPlay_PadEntrance(int song);
/** The song whose pad is @p entrance, -1 for any other entrance. */
int Combo_MMWarpPlay_SongOfPad(uint16_t entrance);

/* ---- The creation's record (ComboOoTWarpRecord) ---------------------------- */

typedef enum {
    COMBO_OOT_WARP_REC_UNSET = 0,   // formatVersion 0: a file created before XW7
    COMBO_OOT_WARP_REC_INVALID = 1, // anything that is not a well-formed v1
    COMBO_OOT_WARP_REC_PRESENT = 2,
} ComboOoTWarpRecordStatus;

/** Every byte zero (UNSET). */
void Combo_OoTWarpRecord_Clear(ComboOoTWarpRecord* rec);
/**
 * Fill @p rec as v1 from the world's answers: @p shuffled / @p hints its Shuffle
 * Warp Songs and Warp Song Hints, @p dest the six pads' resolved entrances,
 * @p area the six hinted areas (written only with @p hints; 0 otherwise). With
 * @p shuffled false every destination is written as its vanilla pad. Returns the
 * record's status (PRESENT unless the inputs were malformed).
 */
int Combo_OoTWarpRecord_Build(ComboOoTWarpRecord* rec, bool shuffled, bool hints, const uint16_t* dest,
                              const uint8_t* area);
/**
 * The status of @p rec: UNSET for formatVersion 0; PRESENT for a v1 whose flags
 * use only bits 0-1, whose HINTS implies SHUFFLED, whose destinations are the
 * vanilla pads when SHUFFLED is clear and nonzero otherwise, and whose areas are
 * all 0 without HINTS and each 0 or 1..Combo_Area_Count(GAME_OOT) with it (0:
 * the destination's region has no OoT area, which the prompt calls "a mysterious
 * place" as OoT does); INVALID otherwise. NULL is UNSET.
 */
int Combo_OoTWarpRecord_StatusOf(const ComboOoTWarpRecord* rec);
/** Combo_OoTWarpRecord_StatusOf over gComboCtx.comboOoTWarps. */
int Combo_OoTWarpRecord_Status(void);
/**
 * THE WARP-SONG RECORD INVARIANT (#1169 XW12). True when a world whose second
 * rules word is @p rules2 BREAKS it: a Majora's Mask warp-song rule is on
 * (RSBS_MMWP_RULE_MASK) while @p rec is not PRESENT. Such a world's logic
 * credits the warps from the entrance graph while play refuses every one of
 * them (COMBO_MMWP_NOT_RECORDED), so no entry admits it: the creation fails
 * (OoT_Creation_FinishPairedHalf), a seed package import and a .redsave load
 * refuse it, and an arrival refuses it by name (Combo_ComboSettingsDivergence).
 * False for every word with the six rules off, whatever the record holds.
 */
bool Combo_MMWarpSongRecordBroken(uint32_t rules2, const ComboOoTWarpRecord* rec);
/** The frozen record's destination of @p song (0 out of range). */
uint16_t Combo_OoTWarpRecord_Dest(int song);
/** The frozen record's area of @p song (0 out of range). */
uint8_t Combo_OoTWarpRecord_Area(int song);
/** The frozen record's flags. */
uint8_t Combo_OoTWarpRecord_Flags(void);

/** Combo_OoTWarpRecord_ArrivalFor's answers. PINNED: randomizer_entrance.c
 *  (a C TU of OoT's that does not include this header) spells them as 0, 1, 2. */
#define RSBS_OOT_WARP_ARRIVAL_NONE 0 // not a warp pad, or the record is not PRESENT: XW3's body
#define RSBS_OOT_WARP_ARRIVAL_PAD 1  // the record says unshuffled: land on the pad
#define RSBS_OOT_WARP_ARRIVAL_DEST 2 // the record says shuffled: land on *outDest
/** What OoT's warp arrival on @p entrance takes from the frozen record
 *  (Entrance_OverrideWarpArrival). *outDest is written only for DEST. */
int Combo_OoTWarpRecord_ArrivalFor(uint16_t entrance, uint16_t* outDest);

/* ---- Ocarina of Time's frozen age ------------------------------------------ */

/** OoT's frozen-half layout, registered by OoT's TU (OoT_SlotMeta_Register,
 *  SaveManager.cpp): the s32 linkAge offset and OoT's own "started" test. */
typedef struct ComboMMWarpOoTDesc {
    uint32_t linkAgeOffset;
    int (*isStarted)(const uint8_t* blob, size_t size);
} ComboMMWarpOoTDesc;
/** Registers OoT's layout; NULL unregisters. */
void Combo_MMWarpPlay_RegisterOoTDesc(const ComboMMWarpOoTDesc* desc);
bool Combo_MMWarpPlay_OoTDescRegistered(void);

#define RSBS_MMWP_AGE_ADULT 0
#define RSBS_MMWP_AGE_CHILD 1
#define RSBS_MMWP_AGE_UNKNOWN -1
/** OoT's age in a half of @p size bytes: CHILD (linkAge 1), ADULT (0), else
 *  UNKNOWN (unregistered, out of range, not started, any other value). */
int Combo_MMWarpPlay_OoTAgeOfBlob(const uint8_t* blob, size_t size);
/** Combo_MMWarpPlay_OoTAgeOfBlob over OoT's frozen half; UNKNOWN when OoT has no
 *  frozen half or that half was frozen as not a live file. */
int Combo_MMWarpPlay_OoTFrozenAge(void);

/* ---- Grotto destinations ------------------------------------------------------ */

/** OoT's readiness probe for a grotto destination (Entrance_WarpArrivalOverridesReady,
 *  registered by OoT_SlotMeta_Register); NULL unregisters. */
void Combo_MMWarpPlay_RegisterOoTReadyProbe(int (*probe)(void));
/** Whether OoT's arrival can land @p dest: any plain entrance; a grotto pseudo
 *  index only when the probe answers nonzero (an unregistered probe answers 0). */
bool Combo_MMWarpPlay_DestResolvable(uint16_t dest);
/** The pure half of Combo_MMWarpPlay_DestResolvable: does an arrival at @p dest
 *  depend on the session's readiness probe (a grotto pseudo index, at or above
 *  RSBS_MMWP_GROTTO_PSEUDO_FIRST)? */
bool Combo_MMWarpPlay_DestNeedsOoTReady(uint16_t dest);
/** May the LOGIC credit an MM-warp arrival at @p dest (#1169 XW9)? Only where play
 *  lands in every session, whatever the probe says: a nonzero destination below
 *  the grotto pseudo indices (graves included: they are real entrances). Pure. */
bool Combo_MMWarpPlay_DestCreditedByLogic(uint16_t dest);
/** #1169 XW9: does OoT's logic open song @p song's warp-song root exit for an MM
 *  arrival? Only inside a coordinator round (@p inRound), only once the round
 *  handed OoT that song's MM fact (bit 0x02 << song of @p accepted,
 *  RSBS_COMBO_TRAVEL_INTO_OOT), and only to a destination @p dest the logic may
 *  credit (Combo_MMWarpPlay_DestCreditedByLogic). The age and time terms (a child
 *  at day) are the region file's. Pure. */
bool Combo_MMWarpPlay_TravelEdgeOpen(bool inRound, uint32_t accepted, int song, uint16_t dest);

/* ---- The prompt's decision -------------------------------------------------- */

typedef struct ComboMMWarpPlayInputs {
    uint8_t song;              // 0..5
    uint8_t paired;            // Combo_ForeignPairingActive()
    uint8_t ootPlayable;       // Switch_GetTargetStatus(GAME_OOT) == SWITCH_TARGET_PLAYABLE
    uint8_t switchPending;     // ComboWarp_IsArmed() || a switch requested or pending
    uint8_t soaringRestricted; // play->interfaceCtx.restrictions.songOfSoaring != 0
    int8_t ootAge;             // Combo_MMWarpPlay_OoTFrozenAge()
    uint8_t recordStatus;      // Combo_OoTWarpRecord_Status()
    uint8_t destResolvable;    // Combo_MMWarpPlay_DestResolvable(Combo_OoTWarpRecord_Dest(song))
} ComboMMWarpPlayInputs;

typedef enum {
    COMBO_MMWP_OFFER = 0,           // the prompt, OK / No
    COMBO_MMWP_OOT_UNAVAILABLE = 1, // unpaired, OoT cannot run now, or a switch is pending (#954 asked first)
    COMBO_MMWP_RESTRICTED = 2,      // the scene restricts the Song of Soaring (OoTMM, ruling 5)
    COMBO_MMWP_NOT_RECORDED = 3,    // the file records no warp-song destinations (created before XW7)
    COMBO_MMWP_NOT_CHILD = 4,       // OoT's frozen half is an adult or unknown (crossing is child-only)
    COMBO_MMWP_NOT_READY = 5,       // a grotto destination OoT cannot resolve now
    COMBO_MMWP_BAD_SONG = 6,        // not one of the six
} ComboMMWarpPlayGate;

/** Every input for @p song, read now from the combo state (pairing, the switch
 *  target's status, the warp target and the pending switch, OoT's frozen age, the
 *  record); @p soaringRestricted is MM's scene restriction, which only MM reads. */
void Combo_MMWarpPlay_GatherInputs(int song, bool soaringRestricted, ComboMMWarpPlayInputs* out);
/** The first failing gate, in the order above; OFFER when every gate holds. Pure. */
ComboMMWarpPlayGate Combo_MMWarpPlay_Decide(const ComboMMWarpPlayInputs* in);
/** A short name for @p gate's stderr line. */
const char* Combo_MMWarpPlay_GateName(ComboMMWarpPlayGate gate);

/* ---- The texts ---------------------------------------------------------------- */

/**
 * Which prompt a frozen record's flags ask for (operator ruling 2026-11-10 on
 * open question A1, "Yes, mysterious"), decided from the record's flags only:
 *   - HINT: Warp Song Hints on (the record's HINTS flag): the frozen hint area,
 *     OoT's RHT_WARP_SONG shape;
 *   - MYSTERIOUS: warp songs SHUFFLED and hints off: OoT's own prompt in that
 *     case, BuildMysteriousWarpMessage (Messages/StaticHints.cpp), "Warp to&%ra
 *     mysterious place?%w", since the vanilla place OoTMM's fixed text names is
 *     not where the song leads in that world;
 *   - FIXED: not shuffled: OoTMM's fixed warpTexts[i], which is true there.
 * The values are Combo_MMWarpPlay_PromptText's return codes.
 */
typedef enum ComboMMWarpPlayPromptForm {
    COMBO_MMWP_PROMPT_FIXED = 0,
    COMBO_MMWP_PROMPT_HINT = 1,
    COMBO_MMWP_PROMPT_MYSTERIOUS = 2,
} ComboMMWarpPlayPromptForm;
/** The pure decision above over a PRESENT record's @p flags (RSBS_OOT_WARP_REC_*). */
ComboMMWarpPlayPromptForm Combo_MMWarpPlay_PromptFormFor(uint8_t flags);

/**
 * The prompt's question for @p song, read from the frozen record only, in 2Ship's
 * CustomMessage colour codes (%r %g %b %y %p %w, plus %t for MM's teal, which the
 * MM TU maps) with '\n' line breaks; the MM TU appends the two-choice. Returns the
 * form Combo_MMWarpPlay_PromptFormFor chose: COMBO_MMWP_PROMPT_HINT (1,
 * "Warp to\n%r<area>%w?", OoT's RHT_WARP_SONG shape; "a mysterious place" for an
 * area 0), COMBO_MMWP_PROMPT_MYSTERIOUS (2, "Warp to\n%ra mysterious place?%w",
 * OoT's BuildMysteriousWarpMessage) or COMBO_MMWP_PROMPT_FIXED (0, OoTMM's fixed
 * warpTexts[i]); -1 when the record is not PRESENT, the song is out of range or
 * the buffer is too small (out is then "").
 */
int Combo_MMWarpPlay_PromptText(int song, char* out, size_t len);
/** "You played the <colour><song>%w!" (OoTMM sCustomSongPlayedText). Returns the
 *  length, or -1 out of range or for a buffer too small. */
int Combo_MMWarpPlay_PlayedText(int song, char* out, size_t len);
/** How many lines Combo_MMWarpPlay_PromptText's text spans ('\n' + 1). */
int Combo_MMWarpPlay_LineCount(const char* text);

/* ---- The one-shot latches (RAM only) ----------------------------------------- */

/** The prompt's pending song: set on OFFER, taken by Player's OCARINA_MODE_WARP arm,
 *  cleared when the ocarina ends. */
void Combo_MMWarpPlay_SetPending(int song);
bool Combo_MMWarpPlay_HasPending(void);
/** The pending song, -1 for none. */
int Combo_MMWarpPlay_PendingSong(void);
/** Takes the pending song into *outSong and clears it; also re-arms the departure
 *  latch below (a new prompt is a new departure). False with nothing pending. */
bool Combo_MMWarpPlay_TakePending(uint8_t* outSong);
void Combo_MMWarpPlay_ClearPending(void);
/** The departure latch: true exactly once per TakePending (or reset), false for
 *  every later call. En_Test7's final action runs every frame until the scene
 *  unloads, so the departure acts on the first call only. */
bool Combo_MMWarpPlay_DepartOnce(void);
/** Clears both latches (an MM file load, a test). */
void Combo_MMWarpPlay_ResetLatches(void);

/* ---- The prompt's action ------------------------------------------------------ */

/** What a played warp song does when its "You played" text closes (MM_Message_Update
 *  TEXT_CLOSING, MM_WarpSongMM_OnSongPlayed). */
typedef struct ComboMMWarpPlayPromptAction {
    uint8_t gate;       // ComboMMWarpPlayGate, Combo_MMWarpPlay_Decide's answer
    uint8_t prompt;     // 1: the OK / No prompt (OCARINA_MODE_ACTIVE); 0: MM's refusal
                        // (OCARINA_MODE_PROCESS_RESTRICTED_SONG)
    uint16_t textId;    // the prompt's box RSBS_MMWP_TEXT_PROMPT_BOX (its text is question),
                        // or the refusal's text RSBS_MMWP_TEXT_RESTRICTED
    int8_t pendingSong; // the pending latch it leaves: the song for the prompt, -1 otherwise
    char question[128]; // Combo_MMWarpPlay_PromptText's question for the prompt, "" otherwise
} ComboMMWarpPlayPromptAction;

/** Clears the pending latch, decides @p in, and on OFFER with a question sets the
 *  pending latch to the song (Player's OCARINA_MODE_WARP arm takes it); every other
 *  answer, and a NULL @p in, is the refusal. @p out NULL: nothing but the clear. */
void Combo_MMWarpPlay_BeginPrompt(const ComboMMWarpPlayInputs* in, ComboMMWarpPlayPromptAction* out);

#ifdef __cplusplus
}
#endif
