/**
 * @file warp_song_soaring.c
 * @brief Ocarina of Time's Song of Soaring to a Termina owl, the game-neutral
 *        half (#1169 lane XW6). The reasoning is in warp_song_soaring.h.
 */

#include "warp_song_soaring.h"
#include "warp_song_soaring_map.h" // Combo_SoaringCloudMaskFromBlob (#1169 XW8)

#include "context.h" // Context_HasFrozenState, Context_GetMMSaveContext
#include "game.h"    // GAME_MM, MM_SAVE_CONTEXT_SIZE

#include <string.h>

typedef struct {
    uint16_t entrance;
    const char* name;
} ComboSoaringOwlRow;

// MM's sOwlWarpEntrances (ovl_En_Test7) in OwlWarpId order, ENTRANCE(scene, spawn)
// = (scene << 9) | (spawn << 4); MM's TU static-asserts each value.
static const ComboSoaringOwlRow kComboSoaringOwls[RSBS_SOARING_OWL_COUNT] = {
    { 0x68B0, "Great Bay Coast" },  // ENTRANCE(GREAT_BAY_COAST, 11)
    { 0x6A60, "Zora Cape" },        // ENTRANCE(ZORA_CAPE, 6)
    { 0xB230, "Snowhead" },         // ENTRANCE(SNOWHEAD, 3)
    { 0x9A80, "Mountain Village" }, // ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 8)
    { 0xD890, "Clock Town" },       // ENTRANCE(SOUTH_CLOCK_TOWN, 9)
    { 0x3E40, "Milk Road" },        // ENTRANCE(MILK_ROAD, 4)
    { 0x8640, "Woodfall" },         // ENTRANCE(WOODFALL, 4)
    { 0x84A0, "Southern Swamp" },   // ENTRANCE(SOUTHERN_SWAMP_POISONED, 10)
    { 0x2040, "Ikana Canyon" },     // ENTRANCE(IKANA_CANYON, 4)
    { 0xAA30, "Stone Tower" },      // ENTRANCE(STONE_TOWER, 3)
};

static const uint8_t kComboSoaringOoTNotes[RSBS_SOARING_NOTE_COUNT] = { 5, 11, 14, 5, 11, 14 };

uint16_t Combo_SoaringOwlEntrance(int owl) {
    if (owl < 0 || owl >= RSBS_SOARING_OWL_COUNT) {
        return 0;
    }
    return kComboSoaringOwls[owl].entrance;
}

const char* Combo_SoaringOwlName(int owl) {
    if (owl < 0 || owl >= RSBS_SOARING_OWL_COUNT) {
        return NULL;
    }
    return kComboSoaringOwls[owl].name;
}

const uint8_t* Combo_SoaringOoTPitches(void) {
    return kComboSoaringOoTNotes;
}

bool Combo_WarpSong_TailMatches(const uint8_t* buf, uint8_t appendPos, const uint8_t* pitches, uint8_t len) {
    if (buf == NULL || pitches == NULL || len == 0 || appendPos < len) {
        return false;
    }
    const uint8_t start = (uint8_t)(appendPos - len);
    for (uint8_t j = 0; j < len; j++) {
        if (buf[start + j] != pitches[j]) {
            return false;
        }
    }
    return true;
}

bool Combo_SoaringOoTTailMatches(const uint8_t* buf, uint8_t appendPos) {
    return Combo_WarpSong_TailMatches(buf, appendPos, kComboSoaringOoTNotes, (uint8_t)RSBS_SOARING_NOTE_COUNT);
}

bool Combo_SoaringMatchArmed(bool ruleOn, bool paired, bool owned, bool child, bool freePlay) {
    return ruleOn && paired && owned && child && freePlay;
}

ComboSoaringPlayed Combo_SoaringOnPlayed(bool freePlay) {
    return freePlay ? COMBO_SOARING_PLAYED_ACCEPT : COMBO_SOARING_PLAYED_FAIL;
}

ComboSoaringGate Combo_SoaringDecide(bool disableWarpSongs, uint8_t warpSongsRestriction, bool mmPlayable,
                                     uint16_t owlMask) {
    // OoTMM SetupSoaring: any nonzero restriction refuses (ruling 5). Vanilla's
    // warp songs refuse only restriction 3, and the randomizer lets even that
    // through; Soaring does not.
    if (disableWarpSongs || warpSongsRestriction != 0) {
        return COMBO_SOARING_GATE_RESTRICTED;
    }
    // #954's question, asked before anything is armed (entrance.h, WHO ASKS FIRST).
    if (!mmPlayable) {
        return COMBO_SOARING_GATE_MM_UNPLAYABLE;
    }
    if ((owlMask & RSBS_SOARING_OWL_BITS) == 0) {
        return COMBO_SOARING_GATE_NO_STATUES;
    }
    return COMBO_SOARING_GATE_OFFER;
}

uint8_t Combo_SoaringOcarinaModeAfterGate(ComboSoaringGate gate) {
    return gate == COMBO_SOARING_GATE_OFFER ? (uint8_t)RSBS_OOT_OCARINA_MODE_ACTIVE
                                            : (uint8_t)RSBS_OOT_OCARINA_MODE_END;
}

int Combo_SoaringFirstOwl(uint16_t owlMask) {
    owlMask &= RSBS_SOARING_OWL_BITS;
    if (owlMask == 0) {
        return -1;
    }
    if ((owlMask >> RSBS_SOARING_OWL_CLOCK_TOWN) & 1u) {
        return RSBS_SOARING_OWL_CLOCK_TOWN;
    }
    for (int owl = 0; owl < RSBS_SOARING_OWL_COUNT; owl++) {
        if ((owlMask >> owl) & 1u) {
            return owl;
        }
    }
    return -1;
}

int Combo_SoaringNextOwl(uint16_t owlMask, int current) {
    owlMask &= RSBS_SOARING_OWL_BITS;
    if (owlMask == 0) {
        return -1;
    }
    if (current < 0 || current >= RSBS_SOARING_OWL_COUNT) {
        current = RSBS_SOARING_OWL_COUNT - 1;
    }
    for (int step = 1; step <= RSBS_SOARING_OWL_COUNT; step++) {
        const int owl = (current + step) % RSBS_SOARING_OWL_COUNT;
        if ((owlMask >> owl) & 1u) {
            return owl;
        }
    }
    return -1;
}

ComboSoaringChoice Combo_SoaringChooserInput(bool advance, bool accept, uint8_t choiceIndex) {
    if (!advance && !accept) {
        return COMBO_SOARING_CHOICE_WAIT;
    }
    if (accept && choiceIndex == 0) {
        return COMBO_SOARING_CHOICE_SOAR;
    }
    if (accept && choiceIndex == 1) {
        return COMBO_SOARING_CHOICE_NEXT;
    }
    return COMBO_SOARING_CHOICE_CANCEL;
}

uint8_t Combo_SoaringOcarinaModeAfterChoice(ComboSoaringChoice choice) {
    return (choice == COMBO_SOARING_CHOICE_WAIT || choice == COMBO_SOARING_CHOICE_NEXT)
               ? (uint8_t)RSBS_OOT_OCARINA_MODE_ACTIVE
               : (uint8_t)RSBS_OOT_OCARINA_MODE_END;
}

static ComboSoaringMMDesc sComboSoaringMMDesc;
static bool sComboSoaringMMDescRegistered;

void Combo_Soaring_RegisterMMDesc(const ComboSoaringMMDesc* desc) {
    if (desc == NULL) {
        memset(&sComboSoaringMMDesc, 0, sizeof(sComboSoaringMMDesc));
        sComboSoaringMMDescRegistered = false;
        return;
    }
    sComboSoaringMMDesc = *desc;
    if (sComboSoaringMMDesc.newfLen > sizeof(sComboSoaringMMDesc.newf)) {
        sComboSoaringMMDesc.newfLen = (uint8_t)sizeof(sComboSoaringMMDesc.newf);
    }
    sComboSoaringMMDescRegistered = true;
}

bool Combo_Soaring_MMDescRegistered(void) {
    return sComboSoaringMMDescRegistered;
}

uint16_t Combo_SoaringOwlMaskFromBlob(const uint8_t* blob, size_t size) {
    if (blob == NULL || !sComboSoaringMMDescRegistered) {
        return 0;
    }
    const ComboSoaringMMDesc* d = &sComboSoaringMMDesc;
    if ((size_t)d->owlFlagsOffset + sizeof(uint16_t) > size || (size_t)d->dayOffset + sizeof(int32_t) > size ||
        (size_t)d->newfOffset + d->newfLen > size) {
        return 0;
    }
    if (d->newfLen == 0 || memcmp(blob + d->newfOffset, d->newf, d->newfLen) != 0) {
        return 0; // no MM half (an all-zero shadow, or the boot chain's unmarked bootstrap)
    }
    int32_t day = 0;
    memcpy(&day, blob + d->dayOffset, sizeof(day));
    if (day < 1) {
        return 0; // a paired half never entered: day 0, 05:59 (decision 3)
    }
    uint16_t owls = 0;
    memcpy(&owls, blob + d->owlFlagsOffset, sizeof(owls));
    return (uint16_t)(owls & RSBS_SOARING_OWL_BITS);
}

uint16_t Combo_SoaringOwlMask(void) {
    if (!Context_HasFrozenState(GAME_MM)) {
        return 0;
    }
    return Combo_SoaringOwlMaskFromBlob((const uint8_t*)Context_GetMMSaveContext(), (size_t)MM_SAVE_CONTEXT_SIZE);
}

// #1169 XW8: the clouds the owl map draws, from the same half under the same
// guard as the owls (warp_song_soaring_map.h).
uint16_t Combo_SoaringCloudMaskFromBlob(const uint8_t* blob, size_t size) {
    const ComboSoaringMMDesc* d = &sComboSoaringMMDesc;
    if (blob == NULL || !sComboSoaringMMDescRegistered || d->cloudsOffset == 0 ||
        (size_t)d->cloudsOffset + sizeof(uint32_t) > size) {
        return 0;
    }
    // The owl mask's guard: a half that is unmarked, on day 0 or out of range
    // offers no owl, so its clouds are never read either.
    if ((size_t)d->dayOffset + sizeof(int32_t) > size || (size_t)d->newfOffset + d->newfLen > size || d->newfLen == 0 ||
        memcmp(blob + d->newfOffset, d->newf, d->newfLen) != 0) {
        return 0;
    }
    int32_t day = 0;
    memcpy(&day, blob + d->dayOffset, sizeof(day));
    if (day < 1) {
        return 0;
    }
    uint32_t clouds = 0;
    memcpy(&clouds, blob + d->cloudsOffset, sizeof(clouds));
    return (uint16_t)(clouds & ((1u << RSBS_SOARING_CLOUD_COUNT) - 1u));
}

uint16_t Combo_SoaringCloudMask(void) {
    if (!Context_HasFrozenState(GAME_MM)) {
        return 0;
    }
    return Combo_SoaringCloudMaskFromBlob((const uint8_t*)Context_GetMMSaveContext(), (size_t)MM_SAVE_CONTEXT_SIZE);
}
