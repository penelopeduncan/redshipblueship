/**
 * @file oot_save_editor_name_test.cpp
 * Lock for the Save Editor's player-name encoding (SoH #7383, backport O35).
 *
 * gSaveContext.playerName holds eight characters in the game's own encoding, and which encoding is named by
 * gSaveContext.ship.filenameLanguage: PAL (digits 0-9, A-Z 10-35, a-z 36-61, space 62, '-' 63, '.' 64) or NTSC
 * (digits 0-9, kana 10-170, A-Z 171-196, a-z 197-222, space 223, '-' 228, '.' 234). The Save Editor's Player Name
 * Language combobox used to change the language byte alone, so every letter of the name turned into a different
 * character. The port re-encodes the name through decodePlayerNameChar / encodePlayerNameChar
 * (games/oot/soh/Enhancements/debugger/debugSaveEditor.cpp) whenever the language changes.
 *
 * OoTSaveEditorNameEncoding (CTest label "redship", dispatch "oot-save-editor-name-encoding"): display-free and
 * ROM-free, it calls the two pure functions only.
 *   1. Every character the tables cover (0-9, A-Z, a-z, space, '-', '.') encodes to its code in each language and
 *      decodes back to itself.
 *   2. Every code the tables map to ASCII decodes to its character and encodes back to the same code, in each
 *      language.
 *   3. Every NTSC kana code decodes to a space (decodePlayerNameChar gives ASCII only, so a language change turns
 *      kana into spaces, as upstream), and a character neither table has encodes to that language's space.
 *   4. The editor's language-change step, reencodePlayerName (decode in the old language, encode in the new one),
 *      carries every covered character of an eight-character name across every pair of PAL, NTSC JPN and NTSC ENG,
 *      and turns an NTSC kana name into spaces.
 *
 * The combobox branch that calls reencodePlayerName needs ImGui, so the row does not reach it; it is one line in
 * DrawInfoTab guarded by a file being loaded, as upstream guards it.
 */

#include "soh/Enhancements/debugger/debugSaveEditor.h"

// At file scope, not inside an extern "C" block: z64.h includes <memory> under C++
// (oot_shared_kokiri_sword_test.cpp says why). SaveManager.h needs its types.
#include <z64.h>

#include "soh/SaveManager.h"

#include <cstdio>

#define OSEN_ASSERT(cond, ...)                                   \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("[TEST] FAIL (%s:%d): ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                                 \
            printf("\n");                                        \
            return 1;                                            \
        }                                                        \
    } while (0)

namespace {

struct NameLanguage {
    const char* name;
    uint8_t id;
    bool pal;
};

const NameLanguage kLanguages[] = {
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

extern "C" int OoTTest_SaveEditorNameEncoding(void) {
    printf("[TEST] oot-save-editor-name-encoding: the Save Editor's player-name encoding round-trips and keeps a "
           "name across a language change (SoH #7383)\n");

    // Leg 1: character -> code -> character.
    for (const NameLanguage& lang : kLanguages) {
        for (const char* p = kCovered; *p != '\0'; p++) {
            const char c = *p;
            const int code = encodePlayerNameChar(c, lang.id);
            OSEN_ASSERT(code == ExpectedCode(c, lang.pal), "%s: '%c' encoded to %d, expected %d", lang.name, c, code,
                        ExpectedCode(c, lang.pal));
            const char back = decodePlayerNameChar((uint8_t)code, lang.id);
            OSEN_ASSERT(back == c, "%s: '%c' -> %d decoded to '%c'", lang.name, c, code, back);
        }
    }
    printf("[TEST]   leg 1: %d characters round-trip through each of the three languages\n",
           (int)(sizeof(kCovered) - 1));

    // Leg 2: code -> character -> code, for every code a table maps to ASCII.
    int codes = 0;
    for (const NameLanguage& lang : kLanguages) {
        for (int code = 0; code < 256; code++) {
            char c = '\0';
            for (const char* p = kCovered; *p != '\0'; p++) {
                if (ExpectedCode(*p, lang.pal) == code) {
                    c = *p;
                }
            }
            if (c == '\0') {
                continue;
            }
            const char decoded = decodePlayerNameChar((uint8_t)code, lang.id);
            OSEN_ASSERT(decoded == c, "%s: code %d decoded to '%c', expected '%c'", lang.name, code, decoded, c);
            const int back = encodePlayerNameChar(decoded, lang.id);
            OSEN_ASSERT(back == code, "%s: code %d -> '%c' encoded to %d", lang.name, code, decoded, back);
            codes++;
        }
    }
    OSEN_ASSERT(codes == 3 * (int)(sizeof(kCovered) - 1), "covered %d codes, expected %d", codes,
                3 * (int)(sizeof(kCovered) - 1));
    printf("[TEST]   leg 2: %d codes round-trip\n", codes);

    // Leg 3: kana show as a space; an uncovered character stores as the language's space.
    for (int code = 10; code <= 170; code++) {
        for (const NameLanguage& lang : kLanguages) {
            if (lang.pal) {
                continue;
            }
            const char decoded = decodePlayerNameChar((uint8_t)code, lang.id);
            OSEN_ASSERT(decoded == ' ', "%s: kana code %d decoded to '%c', expected a space", lang.name, code, decoded);
        }
    }
    const char uncovered[] = { '?', '_', '!', '\0', '~' };
    for (const NameLanguage& lang : kLanguages) {
        for (char c : uncovered) {
            const int code = encodePlayerNameChar(c, lang.id);
            OSEN_ASSERT(code == ExpectedCode(' ', lang.pal), "%s: uncovered character %d encoded to %d, expected %d",
                        lang.name, (int)c, code, ExpectedCode(' ', lang.pal));
        }
    }
    printf("[TEST]   leg 3: kana decode to a space, uncovered characters encode to a space\n");

    // Leg 4: the editor's language-change step, every pair of languages, over eight-character names that between
    // them hold every covered character (the last one padded with spaces).
    const int coveredCount = (int)(sizeof(kCovered) - 1);
    int names = 0;
    for (const NameLanguage& from : kLanguages) {
        for (const NameLanguage& to : kLanguages) {
            for (int start = 0; start < coveredCount; start += 8) {
                char text[8];
                uint8_t name[8];
                for (int i = 0; i < 8; i++) {
                    text[i] = start + i < coveredCount ? kCovered[start + i] : ' ';
                    name[i] = (uint8_t)ExpectedCode(text[i], from.pal);
                }
                reencodePlayerName(name, from.id, to.id);
                for (int i = 0; i < 8; i++) {
                    OSEN_ASSERT(name[i] == ExpectedCode(text[i], to.pal), "'%c' moved %s -> %s as %d, expected %d",
                                text[i], from.name, to.name, name[i], ExpectedCode(text[i], to.pal));
                }
                names++;
            }
        }
    }
    // An NTSC name of kana (codes 10-17) shows as spaces once it is PAL.
    uint8_t kana[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
    reencodePlayerName(kana, NAME_LANGUAGE_NTSC_JPN, NAME_LANGUAGE_PAL);
    for (int i = 0; i < 8; i++) {
        OSEN_ASSERT(kana[i] == ExpectedCode(' ', true), "kana code %d moved NTSC JPN -> PAL as %d, expected %d", 10 + i,
                    kana[i], ExpectedCode(' ', true));
    }
    printf("[TEST]   leg 4: %d names keep their characters across every language change; kana become spaces\n", names);

    printf("[TEST] PASS: oot-save-editor-name-encoding\n");
    return 0;
}
