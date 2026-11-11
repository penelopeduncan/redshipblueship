/**
 * @file oot_soaring_matcher_test.cpp
 * OoT's half of the oot-soaring-matcher row (#1169 lane XW6; the row is
 * src/common/tests/test_warp_song_soaring.c, the matcher's call site is
 * games/oot/src/code/code_800EC960.c's func_800ED200, its question
 * soh/Enhancements/randomizer/SongOfSoaringOoT.cpp's OoT_SoaringOoT_NoteMatched).
 *
 * THE HELPERS drive OoT's REAL free-play checker one note at a time through its
 * non-static globals, the way func_800EE6F4 calls it once a frame: the ocarina
 * is started with func_800ECC04 (the free-play flags, 0x4000 "without staff"),
 * then each note is put into sCurOcarinaBtnVal with the previous note different,
 * no pitch bend and no L, and func_800ED200 runs. The answer is D_80131878, the
 * state Audio_OcaGetPlayingState publishes plus one (a vanilla song i answers
 * i + 1, the Song of Soaring RSBS_OOT_SONG_SOARING + 1). The gates read the live
 * save (age, the song's RAND_INF flag) and OoT_gPlayState's ocarinaAction, which
 * a private PlayState stands in for; the rule and the pairing are the frozen
 * combo record the row sets. Everything touched is restored: the gate inputs
 * (age, the flag, OoT_gPlayState) and every checker global func_800ECC04,
 * func_800ECB7C and func_800ED200 write (SoaringMatcherState), so a later row
 * in the same process starts from the state this one found.
 */

#include <cstdint>
#include <cstring>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
extern PlayState* OoT_gPlayState;
// code_800EC960.c, all non-static there.
void func_800ED200(void);
extern u8 sCurOcarinaSong[8];
extern u8 sOcarinaSongAppendPos;
extern u8 OoT_sOcarinaHasStartedSong;
extern u8 sCurOcarinaBtnVal;
extern u8 sPrevOcarinaNoteVal;
extern u32 sCurOcarinaBtnPress;
extern s8 D_80130F2C;
extern u32 D_80130F3C;
extern u8 D_80131878;
extern u8 sOcarinaInpEnabled;
extern u8 sLearnSongLastBtn;
extern u16 D_80130F4C;
extern u8 D_8013187C;
extern OcarinaStaff OoT_sPlayingStaff;
extern u8 sOcarinaSongNoteStartIdx;
extern u8 sOcarinaSongCnt;
extern u16 sOcarinaAvailSongs;
extern u8 sStaffPlayingPos;
extern u16 sLearnSongPos[0x10];
extern u16 D_8016BA50[0x10];
extern u16 D_8016BA70[0x10];
extern u8 sLearnSongExpectedNote[0x10];
extern u8 sOcarinaNoteValues[5];
extern OcarinaNote sOcarinaSongs[OCARINA_SONG_MAX][20];
}

namespace {

PlayState sSoaringMatcherPlay;

/** Every code_800EC960.c global the start (func_800ECC04, func_800ECB7C) and the
 *  checker (func_800ED200) write, copied field by field. */
struct SoaringMatcherState {
    u8 curSong[8];
    u8 appendPos;
    u8 hasStarted;
    u8 btnVal;
    u8 prevNoteVal;
    u32 btnPress;
    s8 bend;
    u32 seq;
    u8 playingState;
    u8 inpEnabled;
    u8 learnLastBtn;
    u16 bFlags;
    u8 staffMin;
    OcarinaStaff playingStaff;
    u8 noteStartIdx;
    u8 songCnt;
    u16 availSongs;
    u8 staffPos;
    u16 learnPos[0x10];
    u16 timerA[0x10];
    u16 timerB[0x10];
    u8 learnExpected[0x10];
    OcarinaSongInfo scarecrowNotes;
};

void SoaringMatcherSave(SoaringMatcherState* st) {
    std::memcpy(st->curSong, sCurOcarinaSong, sizeof(st->curSong));
    st->appendPos = sOcarinaSongAppendPos;
    st->hasStarted = OoT_sOcarinaHasStartedSong;
    st->btnVal = sCurOcarinaBtnVal;
    st->prevNoteVal = sPrevOcarinaNoteVal;
    st->btnPress = sCurOcarinaBtnPress;
    st->bend = D_80130F2C;
    st->seq = D_80130F3C;
    st->playingState = D_80131878;
    st->inpEnabled = sOcarinaInpEnabled;
    st->learnLastBtn = sLearnSongLastBtn;
    st->bFlags = D_80130F4C;
    st->staffMin = D_8013187C;
    st->playingStaff = OoT_sPlayingStaff;
    st->noteStartIdx = sOcarinaSongNoteStartIdx;
    st->songCnt = sOcarinaSongCnt;
    st->availSongs = sOcarinaAvailSongs;
    st->staffPos = sStaffPlayingPos;
    std::memcpy(st->learnPos, sLearnSongPos, sizeof(st->learnPos));
    std::memcpy(st->timerA, D_8016BA50, sizeof(st->timerA));
    std::memcpy(st->timerB, D_8016BA70, sizeof(st->timerB));
    std::memcpy(st->learnExpected, sLearnSongExpectedNote, sizeof(st->learnExpected));
    st->scarecrowNotes = gOcarinaSongNotes[OCARINA_SONG_SCARECROW];
}

void SoaringMatcherRestore(const SoaringMatcherState* st) {
    std::memcpy(sCurOcarinaSong, st->curSong, sizeof(st->curSong));
    sOcarinaSongAppendPos = st->appendPos;
    OoT_sOcarinaHasStartedSong = st->hasStarted;
    sCurOcarinaBtnVal = st->btnVal;
    sPrevOcarinaNoteVal = st->prevNoteVal;
    sCurOcarinaBtnPress = st->btnPress;
    D_80130F2C = st->bend;
    D_80130F3C = st->seq;
    D_80131878 = st->playingState;
    sOcarinaInpEnabled = st->inpEnabled;
    sLearnSongLastBtn = st->learnLastBtn;
    D_80130F4C = st->bFlags;
    D_8013187C = st->staffMin;
    OoT_sPlayingStaff = st->playingStaff;
    sOcarinaSongNoteStartIdx = st->noteStartIdx;
    sOcarinaSongCnt = st->songCnt;
    sOcarinaAvailSongs = st->availSongs;
    sStaffPlayingPos = st->staffPos;
    std::memcpy(sLearnSongPos, st->learnPos, sizeof(st->learnPos));
    std::memcpy(D_8016BA50, st->timerA, sizeof(st->timerA));
    std::memcpy(D_8016BA70, st->timerB, sizeof(st->timerB));
    std::memcpy(sLearnSongExpectedNote, st->learnExpected, sizeof(st->learnExpected));
    gOcarinaSongNotes[OCARINA_SONG_SCARECROW] = st->scarecrowNotes;
}

/** The song's ownership bit, written directly. Flags_SetRandomizerInf also runs the
 *  OnFlagSet hooks (the randomizer's give queue, the trackers), which have nothing
 *  to serve in a process that loaded no file; the matcher reads only the bit. */
void SoaringMatcherSetOwned(bool owned) {
    const u32 flag = RAND_INF_OBTAINED_SONG_OF_SOARING_OOT;
    if (owned) {
        gSaveContext.ship.randomizerInf[flag >> 4] |= (u16)(1 << (flag & 0xF));
    } else {
        gSaveContext.ship.randomizerInf[flag >> 4] &= (u16)~(1 << (flag & 0xF));
    }
}

/** Play @p count notes into the real checker; the first nonzero D_80131878, or 0.
 *  With @p bendLast the last note is played bent past func_800ED200's 0x15
 *  threshold, so the checker stores it as 0xFF (no pitch). */
int SoaringMatcherRun(const uint8_t* notes, int count, int child, int owned, int freePlay, int bendLast) {
    SoaringMatcherState saved;
    SoaringMatcherSave(&saved);
    PlayState* const prevPlay = OoT_gPlayState;
    const u8 prevAge = (u8)gSaveContext.linkAge;
    const bool prevOwned = Flags_GetRandomizerInf(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT) != 0;

    std::memset(&sSoaringMatcherPlay, 0, sizeof(sSoaringMatcherPlay));
    sSoaringMatcherPlay.msgCtx.ocarinaAction = freePlay ? OCARINA_ACTION_FREE_PLAY : OCARINA_ACTION_CHECK_NOWARP;
    OoT_gPlayState = &sSoaringMatcherPlay;
    gSaveContext.linkAge = child ? LINK_AGE_CHILD : LINK_AGE_ADULT;
    SoaringMatcherSetOwned(owned != 0);

    // The free-play start (z_message_PAL.c, MSGMODE_OCARINA_STARTING:
    // func_800ECC04(sOcarinaSongBitFlags + 0xC000)) with every song's bit.
    func_800ECC04(0x0FFF + 0xC000);
    int answer = 0;
    for (int n = 0; n < count && answer == 0; n++) {
        sCurOcarinaBtnPress = 0;
        D_80130F2C = (bendLast && n == count - 1) ? 0x20 : 0;
        OoT_sOcarinaHasStartedSong = 1;
        sPrevOcarinaNoteVal = (u8)(notes[n] == 0xFE ? 0xFD : 0xFE);
        sCurOcarinaBtnVal = notes[n];
        func_800ED200();
        answer = D_80131878;
    }
    func_800ECC04(0xFFFF); // stop: no checker left running
    SoaringMatcherRestore(&saved);

    SoaringMatcherSetOwned(prevOwned);
    gSaveContext.linkAge = prevAge;
    OoT_gPlayState = prevPlay;
    return answer;
}

} // namespace

/** Plays @p notes (OoT note values) and returns the checker's answer + 1, or 0. */
extern "C" int OoT_SoaringMatcherTest_Play(const uint8_t* notes, int count, int child, int owned, int freePlay) {
    if (notes == nullptr || count <= 0) {
        return 0;
    }
    return SoaringMatcherRun(notes, count, child, owned, freePlay, 0);
}

/** As OoT_SoaringMatcherTest_Play for a child who owns the song, in free play,
 *  with the last note bent (stored as 0xFF by func_800ED200). */
extern "C" int OoT_SoaringMatcherTest_PlayBentLast(const uint8_t* notes, int count) {
    if (notes == nullptr || count <= 0) {
        return 0;
    }
    return SoaringMatcherRun(notes, count, 1, 1, 1, 1);
}

/** Plays vanilla song @p song's own notes (gOcarinaSongNotes) as a child who owns
 *  the Song of Soaring, in free play; -1 for an out-of-range song. */
extern "C" int OoT_SoaringMatcherTest_PlayVanilla(int song) {
    if (song < 0 || song >= OCARINA_SONG_SCARECROW) {
        return -1;
    }
    uint8_t notes[8];
    const int len = gOcarinaSongNotes[song].len;
    if (len <= 0 || len > 8) {
        return -1;
    }
    for (int j = 0; j < len; j++) {
        notes[j] = sOcarinaNoteValues[gOcarinaSongNotes[song].notesIdx[j]];
    }
    return SoaringMatcherRun(notes, len, 1, 1, 1, 0);
}

/**
 * Records @p pierre (8 OoT note values) as Pierre's song (sOcarinaSongs'
 * scarecrow row, which func_800ECC04 turns into the free-play scarecrow song),
 * plays @p notes as a child who owns the Song of Soaring, and restores the row.
 */
extern "C" int OoT_SoaringMatcherTest_PlayWithPierre(const uint8_t* pierre, const uint8_t* notes, int count) {
    if (pierre == nullptr || notes == nullptr || count <= 0) {
        return 0;
    }
    OcarinaNote savedRow[20];
    std::memcpy(savedRow, sOcarinaSongs[OCARINA_SONG_SCARECROW], sizeof(savedRow));
    const OcarinaSongInfo savedNotes = gOcarinaSongNotes[OCARINA_SONG_SCARECROW];
    std::memset(sOcarinaSongs[OCARINA_SONG_SCARECROW], 0, sizeof(savedRow));
    for (int j = 0; j < 8; j++) {
        OcarinaNote& note = sOcarinaSongs[OCARINA_SONG_SCARECROW][j];
        note.noteIdx = pierre[j];
        note.unk_02 = 18;
        note.volume = 80; // a recorded song: row [1]'s volume is not 0xFF
    }
    sOcarinaSongs[OCARINA_SONG_SCARECROW][8].noteIdx = 0xFF;
    for (int j = 9; j < 20; j++) {
        sOcarinaSongs[OCARINA_SONG_SCARECROW][j].noteIdx = 0xFF;
        sOcarinaSongs[OCARINA_SONG_SCARECROW][j].volume = 0xFF;
    }
    const int answer = SoaringMatcherRun(notes, count, 1, 1, 1, 0);
    std::memcpy(sOcarinaSongs[OCARINA_SONG_SCARECROW], savedRow, sizeof(savedRow));
    gOcarinaSongNotes[OCARINA_SONG_SCARECROW] = savedNotes;
    return answer;
}
