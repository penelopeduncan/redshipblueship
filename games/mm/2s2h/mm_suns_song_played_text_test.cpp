/**
 * @file mm_suns_song_played_text_test.cpp
 * Majora's Mask's half of the ROM-free row mm-sun-song-played-text (CTest
 * MMSunSongPlayedText, redship tier, generating nothing; #1587 stage T). The
 * row's common half and the z_message.c anchor are
 * src/common/tests/test_mm_suns_song_played_text.c; the dispatch is
 * src/common/test_runner.cpp.
 *
 * Over a zeroed stand-in play state whose message table holds a stand-in 0x1B78
 * (MM's "You played the Song of Time!" header: box 3, top, no icon) and a stand-in
 * 0x1B7D (MM's empty Sun's Song entry: box 0, bottom, icon 15), it calls the
 * helpers z_message.c's MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN calls:
 *   S1 MM_SunsSongMM_IsPlayedState is true for state 11 alone (0..0x1FF), with
 *      no key set: the patch has no gate (ruling 2.6).
 *   S2 MM_SunsSongMM_PlayedTextId(11) returns CUSTOM_MESSAGE_ID and leaves OoTMM's
 *      text "You played the %ySun's Song%w!" in activeCustomMessage with 0x1B78's
 *      header, not 0x1B7D's.
 *   S3 That entry loaded as the OnOpenText hook loads it: the font buffer holds the
 *      header and "You played the " 0x04 (yellow) "Sun's Song" 0x00 "!" 0xBF.
 *   S4 Every other state 0..0x1FF returns 0x1B72 + state and arms nothing (the
 *      sentinel entry is untouched).
 * The stand-in play state, MM_gPlayState and activeCustomMessage are put back on
 * every exit. Nothing here reads a CVar, OTRGlobals or a hook registry.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_SunsSongPlayedText_RunHeadless(void) {
    printf("[TEST] mm-sun-song-played-text: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <string>

#include "mm_warp_song_play.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Rando/WarpSongsMM.h"

extern "C" {
#include "variables.h"
#include "message_data_static.h" // MessageTableEntry
}

// CustomMessage.cpp's file-scope message the OnOpenText hook loads.
extern CustomMessage::Entry activeCustomMessage;

namespace {

#define MSSPT_ASSERT(cond, code, ...)                                     \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL(%d): mm-sun-song-played-text: ", (code)); \
            printf(__VA_ARGS__);                                          \
            printf(" (%s:%d)\n", __FILE__, __LINE__);                     \
            return (code);                                                \
        }                                                                 \
    } while (0)

// Stand-in table segments: an 11-byte header, then the body (MESSAGE_HEADER_SIZE).
const char kMssptBox1B78[] = "\x03\x00\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                             "You played the \x01Song of Time\x00!\xBF";
const char kMssptEmpty1B7D[] = "\x00\x03\x0F\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                               "\xBF";

int MssptRunLegs(PlayState* play) {
    // S1: the Sun's Song alone, with nothing set.
    for (unsigned state = 0; state < 0x200u; state++) {
        const bool want = state == 11u;
        MSSPT_ASSERT(MM_SunsSongMM_IsPlayedState(state) == want, 10, "S1 state %u answers %d, want %d", state,
                     (int)MM_SunsSongMM_IsPlayedState(state), (int)want);
    }

    // S4 first, so S2's entry is the last one armed: every other state arms nothing.
    activeCustomMessage = CustomMessage::Entry{};
    activeCustomMessage.msg = "(sentinel)";
    activeCustomMessage.textboxType = 0x7F;
    for (unsigned state = 0; state < 0x200u; state++) {
        if (state == 11u) {
            continue;
        }
        const uint16_t id = MM_SunsSongMM_PlayedTextId(state);
        MSSPT_ASSERT(id == (uint16_t)(0x1B72u + state), 40, "S4 state %u continues to 0x%04X, want 0x%04X", state,
                     (unsigned)id, (unsigned)(uint16_t)(0x1B72u + state));
    }
    MSSPT_ASSERT(activeCustomMessage.msg == "(sentinel)" && activeCustomMessage.textboxType == 0x7F, 41,
                 "S4 a state other than the Sun's Song armed a custom message (\"%s\")",
                 activeCustomMessage.msg.c_str());

    // S2: the Sun's Song arms OoTMM's text in 0x1B78's box.
    const uint16_t id = MM_SunsSongMM_PlayedTextId(11u);
    MSSPT_ASSERT(id == (uint16_t)CUSTOM_MESSAGE_ID, 20, "S2 the Sun's Song continues to 0x%04X, want 0x%04X",
                 (unsigned)id, (unsigned)CUSTOM_MESSAGE_ID);
    MSSPT_ASSERT(activeCustomMessage.msg == "You played the %ySun's Song%w!", 21, "S2 the armed text is \"%s\"",
                 activeCustomMessage.msg.c_str());
    MSSPT_ASSERT(activeCustomMessage.textboxType == 3 && activeCustomMessage.textboxYPos == 0 &&
                     activeCustomMessage.icon == 0xFE && activeCustomMessage.nextMessageID == 0xFFFF,
                 22, "S2 the header is box %d, position %d, icon %d, next 0x%04X: not 0x1B78's (3, 0, 254, 0xFFFF)",
                 (int)activeCustomMessage.textboxType, (int)activeCustomMessage.textboxYPos,
                 (int)activeCustomMessage.icon, (unsigned)activeCustomMessage.nextMessageID);

    // S3: loaded as the OnOpenText hook loads it.
    play->msgCtx.msgLength = 0;
    std::memset(play->msgCtx.font.msgBuf.schar, 0, sizeof(play->msgCtx.font.msgBuf.schar));
    CustomMessage::LoadCustomMessageIntoFont(activeCustomMessage);
    static const char kWant[] = "\x03\x00\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                                "You played the \x04Sun's Song\x00!\xBF";
    const int wantLength = (int)sizeof(kWant) - 1;
    MSSPT_ASSERT(play->msgCtx.msgLength == wantLength, 30, "S3 the loaded message is %d bytes, want %d",
                 (int)play->msgCtx.msgLength, wantLength);
    MSSPT_ASSERT(
        std::memcmp(play->msgCtx.font.msgBuf.schar, kWant, (size_t)wantLength) == 0, 31,
        "S3 the font buffer is not 0x1B78's header then \"You played the \" yellow \"Sun's Song\" white \"!\"");
    return 0;
}

} // namespace

extern "C" int MM_SunsSongPlayedText_RunHeadless(void) {
    printf("[TEST] mm-sun-song-played-text: the Sun's Song's own played text in Majora's Mask (#1587 T)\n");

    MessageTableEntry table[3];
    std::memset(table, 0, sizeof(table));
    table[0].textId = 0x1B78;
    table[0].segment = kMssptBox1B78;
    table[0].msgSize = (u32)(sizeof(kMssptBox1B78) - 1);
    table[1].textId = 0x1B7D;
    table[1].segment = kMssptEmpty1B7D;
    table[1].msgSize = (u32)(sizeof(kMssptEmpty1B7D) - 1);
    table[2].textId = 0xFFFF;

    PlayState* const savedPlay = MM_gPlayState;
    const CustomMessage::Entry savedActive = activeCustomMessage;
    PlayState* play = static_cast<PlayState*>(std::calloc(1, sizeof(PlayState)));
    int rc = 1;
    if (play != nullptr) {
        play->msgCtx.messageTableNES = table;
        MM_gPlayState = play;
        rc = MssptRunLegs(play);
    } else {
        printf("[TEST] FAIL(1): mm-sun-song-played-text: could not allocate the stand-in play state\n");
    }

    MM_gPlayState = savedPlay;
    activeCustomMessage = savedActive;
    // LoadCustomMessageIntoFont armed the foreign icon from an Entry with none, so it is disarmed.
    std::free(play);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-sun-song-played-text: PASS (state 11 alone, always; OoTMM's yellow text in 0x1B78's box; "
           "every other song keeps 0x1B72 + song)\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
