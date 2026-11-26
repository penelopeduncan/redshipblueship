#pragma once
/**
 * @file rando_id_list.h
 * @brief One field of a stored randomizer id list, parsed without throwing (#1819).
 *        Game-header-free.
 *
 * OoT's randomizer stores its excluded checks (gRandoSettings.ExcludedLocations),
 * enabled tricks (gRandoSettings.EnabledTricks) and enabled glitches
 * (gRandoSettings.EnabledGlitches) as comma-separated decimal enum values. The
 * menu writes them clean ("12,34"; older builds "12,34,"), but a hand-edited
 * config or preset, or one from a build whose ids differ, can hold an empty
 * field, a word, a number too large for an int, or an id the enum does not
 * have. The readers used std::stoi, which throws on the first two kinds (out of
 * a menu draw nothing catches) and accepts "12abc" as 12 and "-3" as -3.
 *
 * RandoIdList_ParseField accepts a field only when, after optional surrounding
 * spaces or tabs, it is one or more decimal digits whose value lies in
 * [minId, maxIdExclusive). Anything else is rejected and the caller skips the
 * field, as OoTMM's settings validation drops a trick it does not know
 * (packages/core/src/settings/util.ts, validateSettingsStep). For every field
 * the menu writes it returns the value std::stoi returned, so a well-formed list
 * parses to the same set as before.
 *
 * Locked ROM-free by the `oot-menu-robustness` CTest (OoTMenuRobustness).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse one field of a comma-separated id list.
 * @param field the field's first character (need not be NUL-terminated); NULL rejects
 * @param len the field's length in characters
 * @param minId the lowest id accepted
 * @param maxIdExclusive one past the highest id accepted (at most INT32_MAX)
 * @param out receives the id on success; untouched on rejection
 * @return 1 when the field is a valid id, 0 when the caller must skip it
 */
static inline int RandoIdList_ParseField(const char* field, size_t len, int32_t minId, int32_t maxIdExclusive,
                                         int32_t* out) {
    if (field == NULL || out == NULL) {
        return 0;
    }
    size_t begin = 0;
    size_t end = len;
    while (begin < end && (field[begin] == ' ' || field[begin] == '\t')) {
        begin++;
    }
    while (end > begin && (field[end - 1] == ' ' || field[end - 1] == '\t')) {
        end--;
    }
    if (begin == end) {
        return 0;
    }
    int64_t value = 0;
    for (size_t i = begin; i < end; i++) {
        const char c = field[i];
        if (c < '0' || c > '9') {
            return 0;
        }
        value = value * 10 + (c - '0');
        if (value >= (int64_t)maxIdExclusive) {
            return 0;
        }
    }
    if (value < (int64_t)minId) {
        return 0;
    }
    *out = (int32_t)value;
    return 1;
}

#ifdef __cplusplus
}
#endif
