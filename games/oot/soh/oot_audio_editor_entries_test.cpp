/**
 * @file oot_audio_editor_entries_test.cpp
 * ROM-free lock for SoH's Audio Editor backport O12 (SoH #5685, #6608, #7372). CTest row OoTAudioEditorEntries
 * (label redship), `--test oot-audio-editor-entries`.
 *
 * What was missing:
 *  - #5685: Hyrule Field Morning Theme, Seal of Six Sages, the Ocarina of Time ending piece and End Credits I-IV
 *    were in the collection as non-replaceable rows, so no tab showed them. The header also had 0x5E and 0x66
 *    swapped (0x5E is Seal of Six Sages, 0x66 is Ocarina of Time).
 *  - #6608: the new Ending group had no authentic count, so its Randomize All popped from an empty vector, and the
 *    all-groups buttons skipped it.
 *  - #7372: a row set to "None" stores NA_BGM_DISABLED, and the replacement lookup threw that away as an unknown id
 *    and played the original.
 *
 * Legs:
 *  1. The collection has 0x5E as the fanfare "Seal of Six Sages" (key NA_BGM_SEAL_OF_SAGES), 0x66 as the Ending row
 *     "Ocarina of Time", End Credits I-IV as Ending rows, and Hyrule Field Morning as a fanfare, all replaceable and
 *     usable as replacements.
 *  2. The Ending group's authentic count covers its five rows, and the all-groups list holds it.
 *  3. A row whose replacement key holds NA_BGM_DISABLED answers NA_BGM_DISABLED, and an unknown id still falls back
 *     to the original.
 *
 * Leg 3 writes one replacement key and puts it back as it found it. The collection is a local one, built by the
 * production constructor, so AudioCollection::Instance is not touched.
 */

#include <climits>
#include <cstdio>
#include <string>
#include <vector>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (oot_shared_kokiri_sword_test.cpp says why).
#include <z64.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/audio/AudioCollection.h"

// soh/Enhancements/audio/AudioEditor.cpp.
size_t AuthenticCountBySequenceType(SeqType type);
extern std::vector<SeqType> allTypes;

namespace {

int CheckRow(const std::map<uint16_t, SequenceInfo>& map, uint16_t id, const char* label, const char* sfxKey,
             SeqType category) {
    const auto it = map.find(id);
    if (it == map.end()) {
        printf("[TEST] FAIL: leg 1: sequence 0x%02X (%s) is not in the collection\n", id, label);
        return 1;
    }
    const SequenceInfo& info = it->second;
    if (info.label != label || info.sfxKey != sfxKey || info.category != category || !info.canBeReplaced ||
        !info.canBeUsedAsReplacement) {
        printf("[TEST] FAIL: leg 1: sequence 0x%02X is \"%s\" (%s, category 0x%X, replaceable %d, usable %d); want "
               "\"%s\" (%s, category 0x%X, replaceable 1, usable 1)\n",
               id, info.label.c_str(), info.sfxKey.c_str(), (unsigned)info.category, info.canBeReplaced ? 1 : 0,
               info.canBeUsedAsReplacement ? 1 : 0, label, sfxKey, (unsigned)category);
        return 1;
    }
    return 0;
}

} // namespace

extern "C" int OoT_AudioEditorEntries_RunHeadless(void) {
    printf("[TEST] oot-audio-editor-entries: the Audio Editor shows its missing music rows, the Ending group "
           "randomizes, and a row set to None stays silent (SoH #5685, #6608, #7372)\n");

    AudioCollection collection;
    const std::map<uint16_t, SequenceInfo> map = collection.GetAllSequences();
    int failures = 0;

    // Leg 1: the rows #5685 enabled, under their corrected names.
    failures += CheckRow(map, 0x5E, "Seal of Six Sages", "NA_BGM_SEAL_OF_SAGES", SEQ_FANFARE);
    failures += CheckRow(map, 0x66, "Ocarina of Time", "NA_BGM_OCARINA_OF_TIME", SEQ_ENDING);
    failures += CheckRow(map, NA_BGM_STAFF_1, "End Credits I", "NA_BGM_STAFF_1", SEQ_ENDING);
    failures += CheckRow(map, NA_BGM_STAFF_2, "End Credits II", "NA_BGM_STAFF_2", SEQ_ENDING);
    failures += CheckRow(map, NA_BGM_STAFF_3, "End Credits III", "NA_BGM_STAFF_3", SEQ_ENDING);
    failures += CheckRow(map, NA_BGM_STAFF_4, "End Credits IV", "NA_BGM_STAFF_4", SEQ_ENDING);
    failures += CheckRow(map, NA_BGM_FIELD_MORNING, "Hyrule Field Morning Theme", "NA_BGM_FIELD_MORNING", SEQ_FANFARE);
    printf("[TEST] leg 1: %d of 7 rows wrong\n", failures);

    // Leg 2: the Ending group can be randomized and is in the all-groups list.
    size_t endingRows = 0;
    for (const auto& [id, info] : map) {
        if ((info.category & SEQ_ENDING) && info.canBeUsedAsReplacement) {
            endingRows++;
        }
    }
    const size_t endingCount = AuthenticCountBySequenceType(SEQ_ENDING);
    bool inAllTypes = false;
    for (SeqType type : allTypes) {
        if (type == SEQ_ENDING) {
            inAllTypes = true;
        }
    }
    if (endingRows == 0 || endingCount < endingRows || !inAllTypes) {
        printf("[TEST] FAIL: leg 2: Ending has %zu rows, an authentic count of %zu (want at least the rows) and is "
               "%sin the all-groups list: its Randomize All pops from an empty vector\n",
               endingRows, endingCount, inAllTypes ? "" : "not ");
        failures++;
    }
    printf("[TEST] leg 2: Ending rows %zu, authentic count %zu, in all groups %d\n", endingRows, endingCount,
           inAllTypes ? 1 : 0);

    // Leg 3: "None" survives the replacement lookup; an unknown id does not.
    const std::string key = collection.GetCvarKey("NA_BGM_KOKIRI");
    const int saved = CVarGetInteger(key.c_str(), INT_MIN);
    CVarSetInteger(key.c_str(), NA_BGM_DISABLED);
    const uint16_t none = collection.GetReplacementSequence(NA_BGM_KOKIRI);
    CVarSetInteger(key.c_str(), 0x7FFE);
    const uint16_t unknown = collection.GetReplacementSequence(NA_BGM_KOKIRI);
    if (saved == INT_MIN) {
        CVarClear(key.c_str());
    } else {
        CVarSetInteger(key.c_str(), saved);
    }
    if (none != NA_BGM_DISABLED) {
        printf("[TEST] FAIL: leg 3: Kokiri Forest set to None answered 0x%X (want 0x%X): the original plays\n", none,
               NA_BGM_DISABLED);
        failures++;
    }
    if (unknown != NA_BGM_KOKIRI) {
        printf("[TEST] FAIL: leg 3: Kokiri Forest set to the unknown id 0x7FFE answered 0x%X (want 0x%X)\n", unknown,
               NA_BGM_KOKIRI);
        failures++;
    }
    printf("[TEST] leg 3: None -> 0x%X, unknown -> 0x%X\n", none, unknown);

    if (failures != 0) {
        printf("[TEST] oot-audio-editor-entries: FAIL (%d)\n", failures);
        return 1;
    }
    printf("[TEST] oot-audio-editor-entries: PASS\n");
    return 0;
}
