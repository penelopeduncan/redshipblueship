/**
 * @file mm_warp_song_play.c
 * @brief Ocarina of Time's six warp songs played in Majora's Mask (#1169 lane
 *        XW7): the game-neutral half. The reasoning is in mm_warp_song_play.h.
 */

#include "mm_warp_song_play.h"

#include "combo_area.h"    // Combo_Area_Name, Combo_Area_Count
#include "context.h"       // gComboCtx.comboOoTWarps, the frozen OoT half, Switch_GetTargetStatus
#include "entrance.h"      // ComboWarp_IsArmed, the pending-switch questions
#include "foreign_items.h" // RSBS_XITEM2_SONG_*_MM, Combo_ForeignPairingActive
#include "game.h"          // GAME_OOT, OOT_SAVE_CONTEXT_SIZE

#include <stdio.h>
#include <string.h>

RSBS_CTX_STATIC_ASSERT(RSBS_MMWP_RULE_MASK ==
                           (RSBS_XITEM2_SONG_MINUET_MM | RSBS_XITEM2_SONG_BOLERO_MM | RSBS_XITEM2_SONG_SERENADE_MM |
                            RSBS_XITEM2_SONG_REQUIEM_MM | RSBS_XITEM2_SONG_NOCTURNE_MM | RSBS_XITEM2_SONG_PRELUDE_MM),
                       "RSBS_MMWP_RULE_MASK is the six MM warp-song extension rules (#1169 XW2/XW5)");
RSBS_CTX_STATIC_ASSERT(RSBS_MMWP_SONG_COUNT == RSBS_OOT_WARP_REC_SONGS &&
                           RSBS_MMWP_STATE_LAST == RSBS_MMWP_STATE_FIRST + RSBS_MMWP_SONG_COUNT - 1,
                       "one record slot and one staff state per MM warp song");

/* MM's OCARINA_BTN indices (z64ocarina.h), spelled here so this TU stays
 * game-header-free; the MM matcher row checks them against MM's own table. */
enum { MMWP_BTN_A = 0, MMWP_BTN_CD = 1, MMWP_BTN_CR = 2, MMWP_BTN_CL = 3, MMWP_BTN_CU = 4, MMWP_BTN_COUNT = 5 };

typedef struct MMWarpPlaySong {
    const char* name;
    uint16_t pad;
    uint8_t count;
    uint8_t buttons[8];
    const char* colour; // 2Ship's CustomMessage code for OoTMM's colour; %t is MM's teal
    const char* fixed;  // OoTMM's warpTexts[i], colour code excluded
} MMWarpPlaySong;

/* OoTMM mm/ocarina.c: sCustomSongs (buttons), warpTexts (the fixed prompt) and
 * sCustomSongPlayedText's colours, in CUSTOM_SONG_MINUET .. PRELUDE order. The pads
 * are OoT's sWarpSongEntrances order (z_player.c). */
static const MMWarpPlaySong kMMWarpPlaySongs[RSBS_MMWP_SONG_COUNT] = {
    { "Minuet of Forest",
      0x0600u,
      6,
      { MMWP_BTN_A, MMWP_BTN_CU, MMWP_BTN_CL, MMWP_BTN_CR, MMWP_BTN_CL, MMWP_BTN_CR },
      "%g",
      "Warp to the Lost Woods?" },
    { "Bolero of Fire",
      0x04F6u,
      8,
      { MMWP_BTN_CD, MMWP_BTN_A, MMWP_BTN_CD, MMWP_BTN_A, MMWP_BTN_CR, MMWP_BTN_CD, MMWP_BTN_CR, MMWP_BTN_CD },
      "%r",
      "Warp to the Death Mountain\nCrater?" },
    { "Serenade of Water",
      0x0604u,
      5,
      { MMWP_BTN_A, MMWP_BTN_CD, MMWP_BTN_CR, MMWP_BTN_CR, MMWP_BTN_CL },
      "%b",
      "Warp to Lake Hylia?" },
    { "Requiem of Spirit",
      0x01F1u,
      6,
      { MMWP_BTN_A, MMWP_BTN_CD, MMWP_BTN_A, MMWP_BTN_CR, MMWP_BTN_CD, MMWP_BTN_A },
      "%y",
      "Warp to the Desert Colossus?" },
    { "Nocturne of Shadow",
      0x0568u,
      7,
      { MMWP_BTN_CL, MMWP_BTN_CR, MMWP_BTN_CR, MMWP_BTN_A, MMWP_BTN_CL, MMWP_BTN_CR, MMWP_BTN_CD },
      "%p",
      "Warp to the graveyard?" },
    { "Prelude of Light",
      0x05F4u,
      6,
      { MMWP_BTN_CU, MMWP_BTN_CR, MMWP_BTN_CU, MMWP_BTN_CR, MMWP_BTN_CL, MMWP_BTN_CU },
      "%t",
      "Warp to the Temple of Time?" },
};

static bool MmWpSongInRange(int song) {
    return song >= 0 && song < RSBS_MMWP_SONG_COUNT;
}

const uint8_t* Combo_MMWarpPlay_Buttons(int song, uint8_t* outCount) {
    if (!MmWpSongInRange(song)) {
        if (outCount != NULL) {
            *outCount = 0;
        }
        return NULL;
    }
    if (outCount != NULL) {
        *outCount = kMMWarpPlaySongs[song].count;
    }
    return kMMWarpPlaySongs[song].buttons;
}

const char* Combo_MMWarpPlay_SongName(int song) {
    return MmWpSongInRange(song) ? kMMWarpPlaySongs[song].name : NULL;
}

bool Combo_MMWarpPlay_TailMatches(const uint8_t* buf, uint8_t appendPos, const uint8_t* buttons, uint8_t n,
                                  const uint8_t* btnToPitch) {
    if (buf == NULL || buttons == NULL || btnToPitch == NULL || n == 0 || appendPos < n) {
        return false;
    }
    const uint8_t start = (uint8_t)(appendPos - n);
    for (uint8_t j = 0; j < n; j++) {
        if (buttons[j] >= MMWP_BTN_COUNT || buf[start + j] != btnToPitch[buttons[j]]) {
            return false;
        }
    }
    return true;
}

int Combo_MMWarpPlay_MatchSong(const uint8_t* buf, uint8_t appendPos, const uint8_t* btnToPitch, uint8_t usableMask) {
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; song++) {
        if (((usableMask >> song) & 1u) == 0u) {
            continue;
        }
        const MMWarpPlaySong* s = &kMMWarpPlaySongs[song];
        if (Combo_MMWarpPlay_TailMatches(buf, appendPos, s->buttons, s->count, btnToPitch)) {
            return song;
        }
    }
    return -1;
}

bool Combo_MMWarpPlay_WallStealsBeforeEnd(const uint8_t* buttons, uint8_t n, uint8_t usableMask) {
    if (buttons == NULL || n == 0) {
        return false;
    }
    /* The identity map: the wall's notes are staff buttons already. */
    static const uint8_t kIdentity[MMWP_BTN_COUNT] = { 0, 1, 2, 3, 4 };
    for (uint8_t end = 1; end < n; end++) {
        if (Combo_MMWarpPlay_MatchSong(buttons, end, kIdentity, usableMask) >= 0) {
            return true;
        }
    }
    return false;
}

bool Combo_MMWarpPlay_IsStaffState(unsigned state) {
    return state >= RSBS_MMWP_STATE_FIRST && state <= RSBS_MMWP_STATE_LAST;
}

uint8_t Combo_MMWarpPlay_StaffState(int song) {
    return MmWpSongInRange(song) ? (uint8_t)(RSBS_MMWP_STATE_FIRST + (unsigned)song) : (uint8_t)0u;
}

int Combo_MMWarpPlay_SongOfStaffState(unsigned state) {
    return Combo_MMWarpPlay_IsStaffState(state) ? (int)(state - RSBS_MMWP_STATE_FIRST) : -1;
}

bool Combo_MMWarpPlay_IsEnTest7Params(int16_t params) {
    const uint16_t raw = (uint16_t)params;
    return (raw & 0xFF00u) == RSBS_MMWP_ENTEST7_FLAG && (raw & 0x00FFu) < (unsigned)RSBS_MMWP_SONG_COUNT;
}

int Combo_MMWarpPlay_SongOfEnTest7Params(int16_t params) {
    return Combo_MMWarpPlay_IsEnTest7Params(params) ? (int)((uint16_t)params & 0x00FFu) : -1;
}

uint16_t Combo_MMWarpPlay_PadEntrance(int song) {
    return MmWpSongInRange(song) ? kMMWarpPlaySongs[song].pad : (uint16_t)0u;
}

int Combo_MMWarpPlay_SongOfPad(uint16_t entrance) {
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; song++) {
        if (kMMWarpPlaySongs[song].pad == entrance) {
            return song;
        }
    }
    return -1;
}

/* ---- The record ------------------------------------------------------------- */

static uint16_t MmWpRecDestOf(const ComboOoTWarpRecord* rec, int song) {
    return (uint16_t)(rec->destLE[song * 2] | ((uint16_t)rec->destLE[song * 2 + 1] << 8));
}

void Combo_OoTWarpRecord_Clear(ComboOoTWarpRecord* rec) {
    if (rec != NULL) {
        memset(rec, 0, sizeof(*rec));
    }
}

int Combo_OoTWarpRecord_Build(ComboOoTWarpRecord* rec, bool shuffled, bool hints, const uint16_t* dest,
                              const uint8_t* area) {
    if (rec == NULL) {
        return COMBO_OOT_WARP_REC_INVALID;
    }
    Combo_OoTWarpRecord_Clear(rec);
    rec->formatVersion = (uint8_t)RSBS_OOT_WARP_REC_VERSION;
    rec->flags = (uint8_t)((shuffled ? RSBS_OOT_WARP_REC_SHUFFLED : 0u) | (hints ? RSBS_OOT_WARP_REC_HINTS : 0u));
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; song++) {
        const uint16_t d = (shuffled && dest != NULL) ? dest[song] : kMMWarpPlaySongs[song].pad;
        rec->destLE[song * 2] = (uint8_t)(d & 0xFFu);
        rec->destLE[song * 2 + 1] = (uint8_t)(d >> 8);
        rec->area[song] = (hints && area != NULL) ? area[song] : (uint8_t)0u;
    }
    return Combo_OoTWarpRecord_StatusOf(rec);
}

int Combo_OoTWarpRecord_StatusOf(const ComboOoTWarpRecord* rec) {
    if (rec == NULL || rec->formatVersion == 0u) {
        return COMBO_OOT_WARP_REC_UNSET;
    }
    if (rec->formatVersion != RSBS_OOT_WARP_REC_VERSION ||
        (rec->flags & ~(uint8_t)(RSBS_OOT_WARP_REC_SHUFFLED | RSBS_OOT_WARP_REC_HINTS)) != 0u) {
        return COMBO_OOT_WARP_REC_INVALID;
    }
    const bool shuffled = (rec->flags & RSBS_OOT_WARP_REC_SHUFFLED) != 0u;
    const bool hints = (rec->flags & RSBS_OOT_WARP_REC_HINTS) != 0u;
    if (hints && !shuffled) {
        return COMBO_OOT_WARP_REC_INVALID; // SoH enables Warp Song Hints only with the shuffle (settings.cpp)
    }
    const int ootAreas = Combo_Area_Count((uint8_t)GAME_OOT);
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; song++) {
        const uint16_t d = MmWpRecDestOf(rec, song);
        if (!shuffled ? d != kMMWarpPlaySongs[song].pad : d == 0u) {
            return COMBO_OOT_WARP_REC_INVALID;
        }
        const uint8_t a = rec->area[song];
        if (!hints ? a != 0u : (a != 0u && (int)a > ootAreas)) {
            return COMBO_OOT_WARP_REC_INVALID;
        }
    }
    return COMBO_OOT_WARP_REC_PRESENT;
}

int Combo_OoTWarpRecord_Status(void) {
    return Combo_OoTWarpRecord_StatusOf(&gComboCtx.comboOoTWarps);
}

bool Combo_MMWarpSongRecordBroken(uint32_t rules2, const ComboOoTWarpRecord* rec) {
    return (rules2 & (uint32_t)RSBS_MMWP_RULE_MASK) != 0u &&
           Combo_OoTWarpRecord_StatusOf(rec) != COMBO_OOT_WARP_REC_PRESENT;
}

uint16_t Combo_OoTWarpRecord_Dest(int song) {
    return MmWpSongInRange(song) ? MmWpRecDestOf(&gComboCtx.comboOoTWarps, song) : (uint16_t)0u;
}

uint8_t Combo_OoTWarpRecord_Area(int song) {
    return MmWpSongInRange(song) ? gComboCtx.comboOoTWarps.area[song] : (uint8_t)0u;
}

uint8_t Combo_OoTWarpRecord_Flags(void) {
    return gComboCtx.comboOoTWarps.flags;
}

int Combo_OoTWarpRecord_ArrivalFor(uint16_t entrance, uint16_t* outDest) {
    const int song = Combo_MMWarpPlay_SongOfPad(entrance);
    if (song < 0 || Combo_OoTWarpRecord_Status() != COMBO_OOT_WARP_REC_PRESENT) {
        return RSBS_OOT_WARP_ARRIVAL_NONE;
    }
    if ((Combo_OoTWarpRecord_Flags() & RSBS_OOT_WARP_REC_SHUFFLED) == 0u) {
        return RSBS_OOT_WARP_ARRIVAL_PAD;
    }
    if (outDest != NULL) {
        *outDest = Combo_OoTWarpRecord_Dest(song);
    }
    return RSBS_OOT_WARP_ARRIVAL_DEST;
}

/* ---- OoT's frozen age --------------------------------------------------------- */

static ComboMMWarpOoTDesc sMmWpOoTDesc;
static bool sMmWpOoTDescRegistered;

void Combo_MMWarpPlay_RegisterOoTDesc(const ComboMMWarpOoTDesc* desc) {
    if (desc == NULL) {
        memset(&sMmWpOoTDesc, 0, sizeof(sMmWpOoTDesc));
        sMmWpOoTDescRegistered = false;
        return;
    }
    sMmWpOoTDesc = *desc;
    sMmWpOoTDescRegistered = true;
}

bool Combo_MMWarpPlay_OoTDescRegistered(void) {
    return sMmWpOoTDescRegistered;
}

int Combo_MMWarpPlay_OoTAgeOfBlob(const uint8_t* blob, size_t size) {
    if (blob == NULL || !sMmWpOoTDescRegistered || (size_t)sMmWpOoTDesc.linkAgeOffset + sizeof(int32_t) > size) {
        return RSBS_MMWP_AGE_UNKNOWN;
    }
    if (sMmWpOoTDesc.isStarted == NULL || sMmWpOoTDesc.isStarted(blob, size) == 0) {
        return RSBS_MMWP_AGE_UNKNOWN; // no OoT file in these bytes (an all-zero half, a menu's save)
    }
    int32_t age = -1;
    memcpy(&age, blob + sMmWpOoTDesc.linkAgeOffset, sizeof(age));
    if (age == RSBS_MMWP_AGE_CHILD) {
        return RSBS_MMWP_AGE_CHILD;
    }
    if (age == RSBS_MMWP_AGE_ADULT) {
        return RSBS_MMWP_AGE_ADULT;
    }
    return RSBS_MMWP_AGE_UNKNOWN;
}

int Combo_MMWarpPlay_OoTFrozenAge(void) {
    if (!Context_HasFrozenState(GAME_OOT) || Context_FrozenStateIsLiveFile(GAME_OOT) == 0) {
        return RSBS_MMWP_AGE_UNKNOWN;
    }
    return Combo_MMWarpPlay_OoTAgeOfBlob((const uint8_t*)Context_GetOoTSaveContext(), (size_t)OOT_SAVE_CONTEXT_SIZE);
}

/* ---- Grotto destinations ------------------------------------------------------ */

static int (*sMmWpOoTReadyProbe)(void);

void Combo_MMWarpPlay_RegisterOoTReadyProbe(int (*probe)(void)) {
    sMmWpOoTReadyProbe = probe;
}

bool Combo_MMWarpPlay_DestNeedsOoTReady(uint16_t dest) {
    return dest >= RSBS_MMWP_GROTTO_PSEUDO_FIRST;
}

bool Combo_MMWarpPlay_DestCreditedByLogic(uint16_t dest) {
    return dest != 0u && !Combo_MMWarpPlay_DestNeedsOoTReady(dest);
}

bool Combo_MMWarpPlay_TravelEdgeOpen(bool inRound, uint32_t accepted, int song, uint16_t dest) {
    if (!inRound || !MmWpSongInRange(song)) {
        return false;
    }
    return (accepted & (0x02u << song)) != 0u && Combo_MMWarpPlay_DestCreditedByLogic(dest);
}

bool Combo_MMWarpPlay_DestResolvable(uint16_t dest) {
    if (dest == 0u) {
        return false;
    }
    if (!Combo_MMWarpPlay_DestNeedsOoTReady(dest)) {
        return true;
    }
    return sMmWpOoTReadyProbe != NULL && sMmWpOoTReadyProbe() != 0;
}

/* ---- The decision --------------------------------------------------------------- */

void Combo_MMWarpPlay_GatherInputs(int song, bool soaringRestricted, ComboMMWarpPlayInputs* out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->song = MmWpSongInRange(song) ? (uint8_t)song : (uint8_t)0xFFu;
    out->paired = Combo_ForeignPairingActive() ? 1u : 0u;
    out->ootPlayable = Switch_GetTargetStatus(GAME_OOT) == SWITCH_TARGET_PLAYABLE ? 1u : 0u;
    out->switchPending = (ComboWarp_IsArmed() || Combo_IsGameSwitchRequested() || Combo_IsCrossGameSwitch()) ? 1u : 0u;
    out->soaringRestricted = soaringRestricted ? 1u : 0u;
    out->ootAge = (int8_t)Combo_MMWarpPlay_OoTFrozenAge();
    out->recordStatus = (uint8_t)Combo_OoTWarpRecord_Status();
    out->destResolvable = Combo_MMWarpPlay_DestResolvable(Combo_OoTWarpRecord_Dest(song)) ? 1u : 0u;
}

ComboMMWarpPlayGate Combo_MMWarpPlay_Decide(const ComboMMWarpPlayInputs* in) {
    if (in == NULL || !MmWpSongInRange((int)in->song)) {
        return COMBO_MMWP_BAD_SONG;
    }
    if (!in->paired || !in->ootPlayable || in->switchPending) {
        return COMBO_MMWP_OOT_UNAVAILABLE;
    }
    if (in->soaringRestricted) {
        return COMBO_MMWP_RESTRICTED;
    }
    if (in->recordStatus != (uint8_t)COMBO_OOT_WARP_REC_PRESENT) {
        return COMBO_MMWP_NOT_RECORDED;
    }
    if (in->ootAge != RSBS_MMWP_AGE_CHILD) {
        return COMBO_MMWP_NOT_CHILD;
    }
    if (!in->destResolvable) {
        return COMBO_MMWP_NOT_READY;
    }
    return COMBO_MMWP_OFFER;
}

const char* Combo_MMWarpPlay_GateName(ComboMMWarpPlayGate gate) {
    switch (gate) {
        case COMBO_MMWP_OFFER:
            return "offer";
        case COMBO_MMWP_OOT_UNAVAILABLE:
            return "Ocarina of Time cannot be run now (unpaired, not playable, or a switch is pending)";
        case COMBO_MMWP_RESTRICTED:
            return "this scene restricts the Song of Soaring";
        case COMBO_MMWP_NOT_RECORDED:
            return "this file records no warp-song destinations (created before #1169 XW7)";
        case COMBO_MMWP_NOT_CHILD:
            return "Ocarina of Time's frozen half is not a child (crossing is child-only)";
        case COMBO_MMWP_NOT_READY:
            return "the destination is a grotto Ocarina of Time cannot resolve now";
        case COMBO_MMWP_BAD_SONG:
        default:
            return "not a warp song";
    }
}

/* ---- The texts ------------------------------------------------------------------ */

ComboMMWarpPlayPromptForm Combo_MMWarpPlay_PromptFormFor(uint8_t flags) {
    if ((flags & RSBS_OOT_WARP_REC_HINTS) != 0u) {
        return COMBO_MMWP_PROMPT_HINT;
    }
    if ((flags & RSBS_OOT_WARP_REC_SHUFFLED) != 0u) {
        return COMBO_MMWP_PROMPT_MYSTERIOUS;
    }
    return COMBO_MMWP_PROMPT_FIXED;
}

int Combo_MMWarpPlay_PromptText(int song, char* out, size_t len) {
    if (out == NULL || len == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!MmWpSongInRange(song) || Combo_OoTWarpRecord_Status() != COMBO_OOT_WARP_REC_PRESENT) {
        return -1;
    }
    int written;
    const ComboMMWarpPlayPromptForm form = Combo_MMWarpPlay_PromptFormFor(Combo_OoTWarpRecord_Flags());
    if (form == COMBO_MMWP_PROMPT_HINT) {
        // OoT's own prompt (StaticHints.cpp, RHT_WARP_SONG "Warp to&%r<area>%w?"):
        // an area with no name reads as OoT's "a mysterious place".
        const ComboArea area = { (uint8_t)GAME_OOT, Combo_OoTWarpRecord_Area(song) };
        const char* name = area.area != 0u ? Combo_Area_Name(area) : NULL;
        written = snprintf(out, len, "Warp to\n%%r%s%%w?", name != NULL ? name : "a mysterious place");
    } else if (form == COMBO_MMWP_PROMPT_MYSTERIOUS) {
        // Shuffled with hints off (operator ruling A1): OoT's own prompt for the
        // same song in the same file, BuildMysteriousWarpMessage (StaticHints.cpp)
        // "Warp to&%ra mysterious place?%w", the question mark inside the red as
        // there ('&' is SoH's line break, '\n' 2Ship's).
        written = snprintf(out, len, "Warp to\n%%ra mysterious place?%%w");
    } else {
        written = snprintf(out, len, "%s%s%%w", kMMWarpPlaySongs[song].colour, kMMWarpPlaySongs[song].fixed);
    }
    if (written < 0 || (size_t)written >= len) {
        out[0] = '\0';
        return -1;
    }
    return (int)form;
}

int Combo_MMWarpPlay_PlayedText(int song, char* out, size_t len) {
    if (out == NULL || len == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!MmWpSongInRange(song)) {
        return -1;
    }
    const int written =
        snprintf(out, len, "You played the %s%s%%w!", kMMWarpPlaySongs[song].colour, kMMWarpPlaySongs[song].name);
    if (written < 0 || (size_t)written >= len) {
        out[0] = '\0';
        return -1;
    }
    return written;
}

int Combo_MMWarpPlay_LineCount(const char* text) {
    if (text == NULL) {
        return 0;
    }
    int lines = 1;
    for (const char* p = text; *p != '\0'; p++) {
        if (*p == '\n') {
            lines++;
        }
    }
    return lines;
}

/* ---- The latches -------------------------------------------------------------- */

static int8_t sMmWpPendingSong = -1;
static bool sMmWpDepartArmed;

void Combo_MMWarpPlay_SetPending(int song) {
    sMmWpPendingSong = MmWpSongInRange(song) ? (int8_t)song : (int8_t)-1;
}

bool Combo_MMWarpPlay_HasPending(void) {
    return sMmWpPendingSong >= 0;
}

int Combo_MMWarpPlay_PendingSong(void) {
    return (int)sMmWpPendingSong;
}

bool Combo_MMWarpPlay_TakePending(uint8_t* outSong) {
    if (sMmWpPendingSong < 0) {
        return false;
    }
    if (outSong != NULL) {
        *outSong = (uint8_t)sMmWpPendingSong;
    }
    sMmWpPendingSong = -1;
    sMmWpDepartArmed = true;
    return true;
}

void Combo_MMWarpPlay_ClearPending(void) {
    sMmWpPendingSong = -1;
}

bool Combo_MMWarpPlay_DepartOnce(void) {
    if (!sMmWpDepartArmed) {
        return false;
    }
    sMmWpDepartArmed = false;
    return true;
}

void Combo_MMWarpPlay_ResetLatches(void) {
    sMmWpPendingSong = -1;
    sMmWpDepartArmed = false;
}

/* ---- The prompt's action -------------------------------------------------------- */

void Combo_MMWarpPlay_BeginPrompt(const ComboMMWarpPlayInputs* in, ComboMMWarpPlayPromptAction* out) {
    Combo_MMWarpPlay_ClearPending();
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->pendingSong = -1;
    const ComboMMWarpPlayGate gate = Combo_MMWarpPlay_Decide(in);
    out->gate = (uint8_t)gate;
    if (gate == COMBO_MMWP_OFFER &&
        Combo_MMWarpPlay_PromptText((int)in->song, out->question, sizeof(out->question)) >= 0) {
        out->prompt = 1u;
        out->textId = (uint16_t)RSBS_MMWP_TEXT_PROMPT_BOX;
        Combo_MMWarpPlay_SetPending((int)in->song);
        out->pendingSong = (int8_t)Combo_MMWarpPlay_PendingSong();
        return;
    }
    out->textId = (uint16_t)RSBS_MMWP_TEXT_RESTRICTED;
}
