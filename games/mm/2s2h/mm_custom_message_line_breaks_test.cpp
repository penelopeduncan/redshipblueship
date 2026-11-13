/**
 * @file mm_custom_message_line_breaks_test.cpp
 * #1556 (2Ship's AddLineBreaks, backported with #1403) locks.
 *
 * ROW MMCustomMessageLineBreaks (redship tier, no bring-up: AddLineBreaks reads a
 * plain string and MM's static sNESFontWidths table, nothing else).
 *
 * Every string is built from 'a' and ' ', both 8 units wide in sNESFontWidths, so a
 * 300-unit line holds 37 characters and the 38th overflows. A string of 7-letter
 * words joined by single spaces then wraps at every fourth space (index 31, 63,
 * 95, ...), four words to a line, and the positions below follow from that alone.
 *
 *   L1. FORTY WORDS, NINE AUTOMATIC WRAPS. The fourth and the eighth (127, 255) are
 *       box breaks (0x10), the other seven newlines (0x11) at 31, 63, 95, 159, 191,
 *       223, 287; every other position keeps its letter or space, and the length is
 *       unchanged. RED if the fourth wrap's box break lands anywhere but the newline
 *       it replaces (the upstream function overwrote the letter before the
 *       overflowing one), or if the page count does not restart.
 *   L2. AN EXPLICIT BOX BREAK after three wraps restarts the line count: the three
 *       wraps after it (159, 191, 223) stay newlines. RED if the 0x10 branch stops
 *       resetting the count (the first wrap after it would become a box break).
 *   L3. A SPACE THAT OVERFLOWS becomes the newline and adds no width to the next
 *       line: 37 letters, a space, 37 letters wraps once, at the space (37), and the
 *       second word (296 units) fits its line whole. RED if the space's width is
 *       counted on the new line (the second word is broken before its last letter).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <string>
#include <vector>

#include "2s2h/CustomMessage/CustomMessage.h"

namespace {

int sFailures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        printf("[TEST] FAIL: %s\n", what);
        sFailures++;
    }
}

/** @p words seven-letter words joined by single spaces. */
std::string Words(int words) {
    std::string s;
    for (int w = 0; w < words; w++) {
        if (w > 0) {
            s += ' ';
        }
        s += "aaaaaaa";
    }
    return s;
}

/** Every byte of @p laid against @p plain: a listed newline / box-break position
 *  holds that byte (and was a space in @p plain), every other one is unchanged. */
bool LaidOutAs(const std::string& plain, const std::string& laid, const std::vector<size_t>& newlines,
               const std::vector<size_t>& boxBreaks, const char* label) {
    if (laid.size() != plain.size()) {
        printf("[TEST]   %s: length %zu, want %zu\n", label, laid.size(), plain.size());
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < laid.size(); i++) {
        char want = plain[i];
        for (const size_t at : newlines) {
            want = at == i ? '\x11' : want;
        }
        for (const size_t at : boxBreaks) {
            want = at == i ? '\x10' : want;
        }
        if (want != plain[i] && plain[i] != ' ') {
            printf("[TEST]   %s: listed break %zu is not a space in the input\n", label, i);
            ok = false;
        }
        if (laid[i] != want) {
            printf("[TEST]   %s: byte %zu is 0x%02X, want 0x%02X\n", label, i, (unsigned)(unsigned char)laid[i],
                   (unsigned)(unsigned char)want);
            ok = false;
        }
    }
    return ok;
}

size_t CountOf(const std::string& s, char c) {
    size_t n = 0;
    for (const char x : s) {
        n += x == c ? 1 : 0;
    }
    return n;
}

} // namespace

extern "C" int MM_CustomMessageLineBreaks_RunHeadless(void) {
    printf("[TEST] mm-custom-message-line-breaks: MM's AddLineBreaks wraps at spaces, turns every fourth wrap into a "
           "box break, restarts the count at an explicit one, and loses no letter (#1556)\n");
    sFailures = 0;

    // ---- L1 ------------------------------------------------------------------
    {
        const std::string plain = Words(40);
        std::string laid = plain;
        CustomMessage::AddLineBreaks(&laid);
        printf("[TEST] L1: %zu chars in, %zu out, %zu newline(s), %zu box break(s), %zu letter(s)\n", plain.size(),
               laid.size(), CountOf(laid, '\x11'), CountOf(laid, '\x10'), CountOf(laid, 'a'));
        Check(LaidOutAs(plain, laid, { 31, 63, 95, 159, 191, 223, 287 }, { 127, 255 }, "L1"),
              "L1 forty words: newlines at 31 63 95 159 191 223 287, box breaks at 127 255, nothing else moved");
        Check(CountOf(laid, 'a') == 280, "L1 every one of the 280 letters is kept");
    }

    // ---- L2 ------------------------------------------------------------------
    {
        const std::string plain = Words(16) + '\x10' + Words(16);
        std::string laid = plain;
        CustomMessage::AddLineBreaks(&laid);
        printf("[TEST] L2: %zu chars in, %zu out, %zu newline(s), %zu box break(s), %zu letter(s)\n", plain.size(),
               laid.size(), CountOf(laid, '\x11'), CountOf(laid, '\x10'), CountOf(laid, 'a'));
        Check(LaidOutAs(plain, laid, { 31, 63, 95, 159, 191, 223 }, {}, "L2"),
              "L2 three wraps, the explicit box break at 127, three wraps after it, all newlines");
        Check(CountOf(laid, '\x10') == 1 && laid.size() > 127 && laid[127] == '\x10',
              "L2 the explicit box break is the only one");
    }

    // ---- L3 ------------------------------------------------------------------
    {
        const std::string plain = std::string(37, 'a') + ' ' + std::string(37, 'a');
        std::string laid = plain;
        CustomMessage::AddLineBreaks(&laid);
        printf("[TEST] L3: %zu chars in, %zu out, %zu newline(s), %zu letter(s)\n", plain.size(), laid.size(),
               CountOf(laid, '\x11'), CountOf(laid, 'a'));
        Check(LaidOutAs(plain, laid, { 37 }, {}, "L3"),
              "L3 an overflowing space is the one newline, and the 37-letter word after it fits its line");
    }

    if (sFailures != 0) {
        printf("[TEST] FAIL: %d check(s) failed\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: AddLineBreaks places every newline and box break where its width says\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
