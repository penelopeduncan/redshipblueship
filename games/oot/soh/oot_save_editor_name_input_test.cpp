/**
 * @file oot_save_editor_name_input_test.cpp
 * Lock for the Save Editor's player-name text box (Save editor v2, SoH #6474, with SoH #7383 hunks 2 and 3).
 *
 * gSaveContext.playerName holds eight characters in the game's own encoding, named by
 * gSaveContext.ship.filenameLanguage: PAL (digits 0-9, A-Z 10-35, a-z 36-61, space 62, '-' 63, '.' 64) or NTSC
 * (digits 0-9, kana 10-170, A-Z 171-196, a-z 197-222, space 223, '-' 228, '.' 234). Save editor v2 edits the name
 * as an eight-character ASCII text box, and as it first shipped it wrote the typed ASCII bytes straight into the name
 * (so "Link" read back as other characters) and padded only the NUL itself, leaving whatever followed it. SoH #7383
 * decodes the name into the box and encodes the box back through decodePlayerNameChar / encodePlayerNameChar, with
 * every character from the first NUL on stored as a space. This tree runs those two loops as decodePlayerNameInput and
 * storePlayerNameInput (games/oot/soh/Enhancements/debugger/debugSaveEditor.cpp).
 *
 * OoTSaveEditorNameInput (CTest label "redship", dispatch "oot-save-editor-name-input"): display-free and ROM-free,
 * it calls the two pure functions only.
 *   1. A name typed into the box stores each character's code in each of PAL, NTSC JPN and NTSC ENG, and the
 *      characters after the box's NUL are stored as spaces even when the buffer still holds old text there.
 *   2. A name stored in each language decodes back into the box as the same text, NUL-terminated at nine.
 *   3. Every character the tables cover survives store then decode, eight at a time, in every language; a character
 *      neither table has is stored as a space; an NTSC kana name shows as spaces.
 */

#include "soh/Enhancements/debugger/debugSaveEditor.h"

// At file scope, not inside an extern "C" block: z64.h includes <memory> under C++
// (oot_shared_kokiri_sword_test.cpp says why). SaveManager.h needs its types.
#include <z64.h>

#include "soh/SaveManager.h"

#include <cstdio>
#include <cstring>

#define OSNI_ASSERT(cond, ...)                                   \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                 \
            printf("\n");                                        \
            return 1;                                            \
        }                                                        \
    } while (0)

namespace {

struct NameInputLanguage {
    const char* name;
    uint8_t id;
    bool pal;
};

const NameInputLanguage kLanguages[] = {
    { "PAL", NAME_LANGUAGE_PAL, true },
    { "NTSC JPN", NAME_LANGUAGE_NTSC_JPN, false },
    { "NTSC ENG", NAME_LANGUAGE_NTSC_ENG, false },
};

// The code each table gives a covered character, written out from the table layout rather than from the encoder.
int ExpectedCode(char c, bool pal) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return (pal ? 10 : 171) + (c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return (pal ? 36 : 197) + (c - 'a');
    }
    if (c == ' ') {
        return pal ? 62 : 223;
    }
    if (c == '-') {
        return pal ? 63 : 228;
    }
    if (c == '.') {
        return pal ? 64 : 234;
    }
    return -1;
}

const char kCovered[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz -.";

} // namespace

extern "C" int OoTTest_SaveEditorNameInput(void) {
    printf("[TEST] oot-save-editor-name-input: the Save Editor's player-name text box stores and shows the name in "
           "the game's encoding (SoH #6474, #7383)\n");

    // Leg 1: typing into the box. The buffer after the NUL still holds "XY" from earlier text, as ImGui leaves it.
    for (const NameInputLanguage& lang : kLanguages) {
        char input[9] = { 'Z', 'e', 'l', 'd', 'a', '\0', 'X', 'Y', '\0' };
        const char expected[8] = { 'Z', 'e', 'l', 'd', 'a', ' ', ' ', ' ' };
        uint8_t name[8];
        memset(name, 0xAA, sizeof(name));
        storePlayerNameInput(input, lang.id, name);
        for (int i = 0; i < 8; i++) {
            OSNI_ASSERT(name[i] == ExpectedCode(expected[i], lang.pal),
                        "%s: box \"Zelda\" stored character %d as %d, expected %d ('%c')", lang.name, i, name[i],
                        ExpectedCode(expected[i], lang.pal), expected[i]);
        }

        // A full box (no NUL among the eight) stores all eight.
        const char full[9] = "Ab-.9 zZ";
        storePlayerNameInput(full, lang.id, name);
        for (int i = 0; i < 8; i++) {
            OSNI_ASSERT(name[i] == ExpectedCode(full[i], lang.pal), "%s: box \"%s\" stored '%c' as %d, expected %d",
                        lang.name, full, full[i], name[i], ExpectedCode(full[i], lang.pal));
        }
    }
    printf("[TEST]   leg 1: a typed name stores the language's codes, padded with spaces after the NUL\n");

    // Leg 2: a stored name shows in the box.
    for (const NameInputLanguage& lang : kLanguages) {
        const char text[9] = "Link -.0";
        uint8_t name[8];
        for (int i = 0; i < 8; i++) {
            name[i] = (uint8_t)ExpectedCode(text[i], lang.pal);
        }
        char input[9];
        memset(input, '#', sizeof(input));
        decodePlayerNameInput(name, lang.id, input);
        OSNI_ASSERT(input[8] == '\0', "%s: the box is not NUL-terminated at nine", lang.name);
        OSNI_ASSERT(memcmp(input, text, 8) == 0, "%s: stored \"%s\" showed as \"%.8s\"", lang.name, text, input);
    }
    printf("[TEST]   leg 2: a stored name shows as its text\n");

    // Leg 3: store then show, for every covered character, eight at a time.
    const int coveredCount = (int)(sizeof(kCovered) - 1);
    int names = 0;
    for (const NameInputLanguage& lang : kLanguages) {
        for (int start = 0; start < coveredCount; start += 8) {
            char text[9];
            for (int i = 0; i < 8; i++) {
                text[i] = start + i < coveredCount ? kCovered[start + i] : ' ';
            }
            text[8] = '\0';
            uint8_t name[8];
            storePlayerNameInput(text, lang.id, name);
            char shown[9];
            decodePlayerNameInput(name, lang.id, shown);
            OSNI_ASSERT(strcmp(shown, text) == 0, "%s: \"%s\" came back as \"%s\"", lang.name, text, shown);
            names++;
        }

        // Characters neither table has are stored as the language's space.
        const char uncovered[9] = "?_!~@#$%";
        uint8_t name[8];
        storePlayerNameInput(uncovered, lang.id, name);
        for (int i = 0; i < 8; i++) {
            OSNI_ASSERT(name[i] == ExpectedCode(' ', lang.pal), "%s: uncovered '%c' stored as %d, expected %d",
                        lang.name, uncovered[i], name[i], ExpectedCode(' ', lang.pal));
        }
    }
    // An NTSC kana name (codes 10-17) shows as spaces.
    const uint8_t kana[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
    char shown[9];
    decodePlayerNameInput(kana, NAME_LANGUAGE_NTSC_JPN, shown);
    OSNI_ASSERT(strcmp(shown, "        ") == 0, "an NTSC kana name showed as \"%s\", expected eight spaces", shown);
    printf("[TEST]   leg 3: %d names survive store then show; uncovered characters and kana become spaces\n", names);

    printf("[TEST] PASS: oot-save-editor-name-input\n");
    return 0;
}
