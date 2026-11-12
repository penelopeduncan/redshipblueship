#pragma once
/**
 * @file warp_song_soaring.h
 * @brief Ocarina of Time's Song of Soaring to a Termina owl (#1169 lane XW6;
 *        OoTMM packages/generator/src/oot/ocarina.c). Game-header-free.
 *
 * THE MATCHER. OoTMM never widens OoT's u16 song masks (their top bits are mode
 * flags: func_800ECC04 keeps `flg & 0x3FFF`, 0x4000 is "without staff"). It
 * compares the tail of the rolling note buffer with a custom song instead
 * (src/common/ocarina.c, comboCheckSong) and, on a match, stops ocarina input and
 * publishes the staff state 0x80 + 1 + i; Soaring is i = 1, so the message code
 * reads RSBS_OOT_SONG_SOARING. Combo_WarpSong_TailMatches is that loop; OoT's call
 * site is the free-play checker (games/oot/src/code/code_800EC960.c,
 * func_800ED200), after the vanilla loop and only when no vanilla song matched
 * on the same note. Lane XW7 reuses the loop with Majora's Mask's note values.
 *
 * THE GATES (operator rulings of 2026-11-04 on #1169, and OoTMM SetupSoaring):
 *   - the song only MATCHES for a child who owns it, in free play, with the
 *     frozen rule on and a paired world (OoTMM gates the match on ownership and
 *     age: an adult plays notes that match nothing);
 *   - any nonzero warp-song restriction of the scene refuses it with OoT's own
 *     "You can't warp here!" (0x88C; ruling 5, OoTMM `restrictions.warpSongs`),
 *     and so does a Majora's Mask this session cannot run (#954's question,
 *     asked before anything is armed);
 *   - with no active owl in MM's frozen copy it shows OoTMM's "no statues" text
 *     and nothing else happens;
 *   - otherwise it offers exactly the active owls.
 *
 * THE OWLS. Combo_SoaringOwlEntrance is MM's own owl table (OwlWarpId order,
 * ovl_En_Test7 sOwlWarpEntrances), the UNCLEARED entrances: MM's Play_Init
 * resolves the cleared swamp and the spring mountain for every startup entrance
 * (nextCutsceneIndex 0xFFEF), as En_Test7 does for its own warp. MM's TU
 * static-asserts every value against ENTRANCE(...).
 *
 * THE FROZEN COPY. The owl flags and the day are read from Majora's Mask's frozen
 * half at offsets MM registers (MM_SlotMeta_Register), never from an MM header.
 * One-game semantics: what OoT offers is what MM's half holds, so an owl
 * activated in MM counts from the next departure from MM.
 *
 * OFFERED SINCE XW12. The rule is RSBS_XITEM2_SONG_SOARING_OOT, a Cross-Game Songs
 * row, off by default: with it off the song never matches and nothing here runs.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The staff state OoT's message code reads for the Song of Soaring (OoTMM: 0x80
 *  + custom song index 1). Above every vanilla state (0..13, 0xFE, 0xFF never
 *  reach it), so no vanilla branch takes it. */
#define RSBS_OOT_SONG_SOARING 0x81u

/** OoT text ids for the three Soaring texts, after SoH's last custom id
 *  (TEXT_CARPET_SALESMAN_ARMS_DEALER 0x9212; SongOfSoaringOoT.cpp static-asserts
 *  the order). Built by OnOpenText hooks, never read from OoT's table. */
#define RSBS_OOT_TEXT_SOARING_PLAYED 0x9213u     // "You played the Song of Soaring."
#define RSBS_OOT_TEXT_SOARING_NO_STATUES 0x9214u // OoTMM's no-statues text
#define RSBS_OOT_TEXT_SOARING_CHOOSER 0x9215u    // "Soar to <owl>?" Soar / Next statue / No

/** The kind of game switch a Soaring warp arms: entrance.h's
 *  COMBO_SWITCH_KIND_SOARING (test_warp_song_soaring.c and MM's TU
 *  static-assert it). Spelled here for the OoT TU, which cannot include
 *  src/common/entrance.h by that name. */
#define RSBS_SOARING_SWITCH_KIND 2u

/** The song's six notes. */
#define RSBS_SOARING_NOTE_COUNT 6u

/** MM's ten owl statues (OWL_WARP_GREAT_BAY_COAST .. OWL_WARP_STONE_TOWER). */
#define RSBS_SOARING_OWL_COUNT 10
#define RSBS_SOARING_OWL_BITS 0x03FFu
#define RSBS_SOARING_OWL_CLOCK_TOWN 4

/** OoT's ocarina modes the gates hand back, game-free (OCARINA_MODE_01 and
 *  OCARINA_MODE_04; SongOfSoaringOoT.cpp static-asserts both). */
#define RSBS_OOT_OCARINA_MODE_ACTIVE 1u
#define RSBS_OOT_OCARINA_MODE_END 4u

/** The owl's MM entrance (uncleared variant), 0 out of range. */
uint16_t Combo_SoaringOwlEntrance(int owl);
/** The owl's English name (OoTMM kSoarNames, MM's own names), NULL out of range. */
const char* Combo_SoaringOwlName(int owl);

/** OoT's note values for the Song of Soaring: C-down, C-left, C-up, twice (OoT's
 *  sOcarinaNoteValues F4 5, B4 11, D5 14). RSBS_SOARING_NOTE_COUNT entries. */
const uint8_t* Combo_SoaringOoTPitches(void);

/**
 * OoTMM's comboCheckSong: whether the last @p len notes of @p buf (whose next
 * free position is @p appendPos, so the notes are buf[0..appendPos-1]) equal
 * @p pitches. False for a NULL argument, a zero length or fewer notes than len.
 */
bool Combo_WarpSong_TailMatches(const uint8_t* buf, uint8_t appendPos, const uint8_t* pitches, uint8_t len);
/** Combo_WarpSong_TailMatches with OoT's Soaring notes. */
bool Combo_SoaringOoTTailMatches(const uint8_t* buf, uint8_t appendPos);

/** Whether a completed Soaring pattern counts as the song: every gate true. */
bool Combo_SoaringMatchArmed(bool ruleOn, bool paired, bool owned, bool child, bool freePlay);

/** What OCARINA_PLAYING does with RSBS_OOT_SONG_SOARING (z_message_PAL.c, E1). */
typedef enum {
    COMBO_SOARING_PLAYED_ACCEPT = 0, // free play: the song-played box (0x86F), like a vanilla song
    COMBO_SOARING_PLAYED_FAIL = 1,   // anything else: the ocarina fails and restarts
} ComboSoaringPlayed;
ComboSoaringPlayed Combo_SoaringOnPlayed(bool freePlay);

/** What SONG_PLAYED_ACT does once "You played the Song of Soaring." has shown. */
typedef enum {
    COMBO_SOARING_GATE_OFFER = 0,         // open the owl chooser
    COMBO_SOARING_GATE_RESTRICTED = 1,    // 0x88C: disableWarpSongs or any nonzero restriction
    COMBO_SOARING_GATE_MM_UNPLAYABLE = 2, // 0x88C: this session cannot run Majora's Mask
    COMBO_SOARING_GATE_NO_STATUES = 3,    // OoTMM's no-statues text
} ComboSoaringGate;
ComboSoaringGate Combo_SoaringDecide(bool disableWarpSongs, uint8_t warpSongsRestriction, bool mmPlayable,
                                     uint16_t owlMask);
/** The ocarina mode a gate leaves: ACTIVE while the chooser is open, END for
 *  every refusal (END hands control back to Link, as vanilla's refusal does). */
uint8_t Combo_SoaringOcarinaModeAfterGate(ComboSoaringGate gate);

/** The chooser's first owl: Clock Town when active (OoTMM's cursor), else the
 *  lowest active bit; -1 when none is active. */
int Combo_SoaringFirstOwl(uint16_t owlMask);
/** The next active owl after @p current in bit order, wrapping; -1 when none. */
int Combo_SoaringNextOwl(uint16_t owlMask, int current);

/** The chooser's answer this frame (Soar / Next statue / No, choice 0 / 1 / 2). */
typedef enum {
    COMBO_SOARING_CHOICE_WAIT = 0,   // the box did not advance this frame
    COMBO_SOARING_CHOICE_SOAR = 1,   // A on "Soar"
    COMBO_SOARING_CHOICE_NEXT = 2,   // A on "Next statue"
    COMBO_SOARING_CHOICE_CANCEL = 3, // A on "No", B (OoTMM's picker cancels on B), C-up, any other choice
} ComboSoaringChoice;
/** @p advance: the message system advances the box this frame (A, B or C-up);
 *  @p accept: A was pressed this frame. Only A selects. */
ComboSoaringChoice Combo_SoaringChooserInput(bool advance, bool accept, uint8_t choiceIndex);
/** The ocarina mode a choice leaves: ACTIVE while the chooser stays open (WAIT,
 *  NEXT), END otherwise (a cancel returns control; a departure freezes Link). */
uint8_t Combo_SoaringOcarinaModeAfterChoice(ComboSoaringChoice choice);

/** MM's frozen-half layout, registered by MM's TU (MM_SlotMeta_Register). */
typedef struct ComboSoaringMMDesc {
    uint32_t owlFlagsOffset; // u16 save.saveInfo.playerData.owlActivationFlags
    uint32_t dayOffset;      // s32 save.day
    uint32_t newfOffset;     // save.saveInfo.playerData.newf, the "ZELDA3" marker
    uint8_t newfLen;
    char newf[6];
    uint32_t cloudsOffset; // u32 save.saveInfo.worldMapCloudVisibility (#1169 XW8); 0: not registered
} ComboSoaringMMDesc;
/** Registers MM's layout; NULL unregisters. */
void Combo_Soaring_RegisterMMDesc(const ComboSoaringMMDesc* desc);
bool Combo_Soaring_MMDescRegistered(void);

/**
 * The active owls (bits 0..9) in an MM half of @p size bytes: 0 unless the
 * layout is registered and in range, the half carries the marker and its day is
 * 1 or later (a paired half never entered is day 0, which a Soaring arrival
 * cannot honour: decision 3 of the PR). Bit 15, MM's hidden owl, is ignored.
 */
uint16_t Combo_SoaringOwlMaskFromBlob(const uint8_t* blob, size_t size);
/** Combo_SoaringOwlMaskFromBlob over Majora's Mask's frozen half; 0 when MM has
 *  no frozen half (Context_HasFrozenState). */
uint16_t Combo_SoaringOwlMask(void);

#ifdef __cplusplus
}
#endif
