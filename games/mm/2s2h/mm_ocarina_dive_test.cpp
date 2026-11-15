/**
 * @file mm_ocarina_dive_test.cpp
 *
 * ROM-free, display-free lock for backport M8, 2Ship2Harkinian's ocarina-dive trio:
 *   - #1842 (efee75149): an always-on Restorations TU re-queues the promoted player-ocarina cutscene for up to two
 *     extra frames from OnGameStateMainStart;
 *   - #1847 (9afe75860): only while Player hovers (GROUND_TOUCH and meleeWeaponState == 1);
 *   - #1853 (e9c42eed9): Faster Song Playback leaves a song's stateTimer alone while Player is falling.
 *
 * CTest name: MMOcarinaDive (`redship --test mm-ocarina-dive`), label "redship".
 *
 * The Restorations TU has no key: it registers through a file-scope RegisterShipInitFunc on "*" only, so it lives in
 * 2ship_enh_registrars and is attributed here by registry content, never by a symbol it defines (a reference from this
 * TU would un-elide it by itself). Legs:
 *   (1) After MM_Rando_Init (S2H::ShipInit::InitAll), with the three keyed OnGameStateMainStart registrants held off
 *       (Easy Frame Advance, Pause Buffer Window, Ammo Buyback), the unkeyed OnGameStateMainStart registry holds
 *       exactly one registrant. Without the TU it holds none.
 *   (2) With MM_gPlayState NULL that registrant queues nothing.
 *   (3) Over a zeroed stand-in play state whose player-ocarina cutscene is promoted (its sNextCutsceneList bit set):
 *       not hovering (no GROUND_TOUCH, or meleeWeaponState != 1) queues nothing; hovering queues it on two frames
 *       running, not on the third (the grace is spent and resets), and again on the fourth; a cutscene that is not
 *       next queues nothing.
 *   (4) Faster Song Playback on: its one OnActorUpdate[ACTOR_PLAYER] registrant, called on a stand-in Player in
 *       MSGMODE_SONG_PLAYED with stateTimer 5, cuts the timer to 1 on the ground and while rising (non-vacuity), and
 *       leaves it at 5 while falling.
 * Restores MM_gPlayState, both cutscene bit lists, MM_sPlaybackState and every key it touched.
 */

#ifndef RSBS_SINGLE_EXECUTABLE
#include <cstdio>
extern "C" int MM_OcarinaDive_RunHeadless(void) {
    printf("[TEST] mm-ocarina-dive: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include "global.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
// z_eventmgr.c's cutscene bit lists (CutsceneManager_Queue writes the first, CutsceneManager_IsNext reads the second).
extern u8 sWaitingCutsceneList[16];
extern u8 sNextCutsceneList[16];
// The song playback state Faster Song Playback clears; saved and restored around leg 4.
extern u8 MM_sPlaybackState;
void MM_Rando_Init(void);
}

namespace {

#define OD_ASSERT(cond, code, msg)                                                                       \
    do {                                                                                                 \
        if (!(cond)) {                                                                                   \
            printf("[TEST] mm-ocarina-dive: FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                                               \
        }                                                                                                \
    } while (0)

// The three keyed OnGameStateMainStart registrants in games/mm; held off so leg 1's count is the always-on one alone.
const char* const kKeyedMainStartKeys[] = {
    "gCheats.EasyFrameAdvance",
    "gEnhancements.PauseBufferWindow",
    "gEnhancements.Items.AmmoBuyback",
};
constexpr const char* kFasterSongPlaybackKey = "gEnhancements.Songs.FasterSongPlayback";

// The player-ocarina cutscene id the stand-in play state carries (any id below 128 fits the 16-byte bit lists).
constexpr s16 kOcarinaCsId = 9;

template <typename H> std::map<uint32_t, typename H::fn> Settled() {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    return S2H::GameHooks::Registry<H>::functions;
}

template <typename H> std::map<uint32_t, typename H::fn> SettledForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? std::map<uint32_t, typename H::fn>{} : it->second;
}

bool BitSet(const u8* list, s16 csId) {
    return (list[csId >> 3] & (1 << (csId & 7))) != 0;
}

bool ListEmpty(const u8* list) {
    for (int i = 0; i < 16; i++) {
        if (list[i] != 0) {
            return false;
        }
    }
    return true;
}

struct Saved {
    PlayState* play = nullptr;
    u8 waiting[16] = {};
    u8 next[16] = {};
    u8 playbackState = 0;
    std::vector<std::pair<std::string, int32_t>> keys;
};

void SetKey(const char* key, int32_t value) {
    if (value == 0) {
        CVarClear(key);
    } else {
        CVarSetInteger(key, value);
    }
    S2H::ShipInit::Init(key);
}

void Restore(const Saved& saved) {
    MM_gPlayState = saved.play;
    memcpy(sWaitingCutsceneList, saved.waiting, sizeof(saved.waiting));
    memcpy(sNextCutsceneList, saved.next, sizeof(saved.next));
    MM_sPlaybackState = saved.playbackState;
    for (const auto& [key, value] : saved.keys) {
        SetKey(key.c_str(), value);
    }
}

int Run(Saved& saved) {
    // ---- Leg 1: the always-on registrant is in the registry -----------------------------------------------------
    MM_Rando_Init(); // S2H::ShipInit::InitAll (irreversible; idempotent within a process)
    for (const char* key : kKeyedMainStartKeys) {
        SetKey(key, 0);
    }
    const auto mainStart = Settled<GameInteractor::OnGameStateMainStart>();
    if (mainStart.size() != 1) {
        printf("[TEST] mm-ocarina-dive: OnGameStateMainStart holds %zu registrants with its keyed ones off\n",
               mainStart.size());
    }
    OD_ASSERT(mainStart.size() == 1, 1,
              "the unkeyed OnGameStateMainStart registry must hold exactly the always-on ocarina-dive registrant "
              "(Restorations/OcarinaDive.cpp linked through 2ship_enh_registrars)");
    const auto dive = mainStart.begin()->second;
    printf("[TEST] mm-ocarina-dive: leg 1: one always-on OnGameStateMainStart registrant\n");

    // ---- Leg 2: no play state, no queue ---------------------------------------------------------------------------
    memset(sWaitingCutsceneList, 0, sizeof(sWaitingCutsceneList));
    memset(sNextCutsceneList, 0, sizeof(sNextCutsceneList));
    MM_gPlayState = nullptr;
    dive();
    OD_ASSERT(ListEmpty(sWaitingCutsceneList), 2, "the registrant queued a cutscene with MM_gPlayState NULL");
    printf("[TEST] mm-ocarina-dive: leg 2: no play state queues nothing\n");

    // ---- Leg 3: the grace frames, hover only -----------------------------------------------------------------------
    // Value-initialised, so both stand-ins start zeroed.
    auto play = std::make_unique<PlayState>();
    auto player = std::make_unique<Player>();
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
    for (int i = 0; i < PLAYER_CS_ID_MAX; i++) {
        play->playerCsIds[i] = CS_ID_NONE;
    }
    play->playerCsIds[PLAYER_CS_ID_ITEM_OCARINA] = kOcarinaCsId;
    MM_gPlayState = play.get();
    sNextCutsceneList[kOcarinaCsId >> 3] |= 1 << (kOcarinaCsId & 7);

    auto frame = [&]() {
        memset(sWaitingCutsceneList, 0, sizeof(sWaitingCutsceneList));
        dive();
        return BitSet(sWaitingCutsceneList, kOcarinaCsId);
    };

    // Not hovering, three ways. Each also resets the grace counter, so the hover legs below start from zero.
    player->actor.bgCheckFlags = 0;
    player->meleeWeaponState = 0;
    OD_ASSERT(!frame(), 3,
              "the promoted ocarina cutscene was re-queued with Player neither on the ground nor slashing");
    player->actor.bgCheckFlags = BGCHECKFLAG_GROUND_TOUCH;
    player->meleeWeaponState = 0;
    OD_ASSERT(!frame(), 4, "the promoted ocarina cutscene was re-queued without meleeWeaponState == 1 (#1847)");
    player->actor.bgCheckFlags = 0;
    player->meleeWeaponState = 1;
    OD_ASSERT(!frame(), 5, "the promoted ocarina cutscene was re-queued without BGCHECKFLAG_GROUND_TOUCH (#1847)");
    printf("[TEST] mm-ocarina-dive: leg 3: not hovering re-queues nothing\n");

    // Hovering: two grace frames, then the counter resets.
    player->actor.bgCheckFlags = BGCHECKFLAG_GROUND_TOUCH;
    player->meleeWeaponState = 1;
    OD_ASSERT(frame(), 6, "a hover did not re-queue the promoted ocarina cutscene on its first grace frame (#1842)");
    OD_ASSERT(frame(), 7, "a hover did not re-queue the promoted ocarina cutscene on its second grace frame (#1842)");
    OD_ASSERT(!frame(), 8, "a hover re-queued the promoted ocarina cutscene on a third frame: the grace is two frames");
    OD_ASSERT(frame(), 9, "the grace counter did not reset after the two grace frames were spent");
    // Not next: nothing to keep alive.
    sNextCutsceneList[kOcarinaCsId >> 3] &= ~(1 << (kOcarinaCsId & 7));
    OD_ASSERT(!frame(), 10, "an ocarina cutscene that is not next was queued");
    printf("[TEST] mm-ocarina-dive: leg 3: a hover gets exactly two grace frames, then resets\n");

    // ---- Leg 4: Faster Song Playback leaves a falling song alone (#1853) -------------------------------------------
    SetKey(kFasterSongPlaybackKey, 0);
    const auto before = SettledForId<GameInteractor::OnActorUpdate>(ACTOR_PLAYER);
    SetKey(kFasterSongPlaybackKey, 1);
    const auto after = SettledForId<GameInteractor::OnActorUpdate>(ACTOR_PLAYER);
    std::vector<GameInteractor::OnActorUpdate::fn> added;
    for (const auto& [id, fn] : after) {
        if (before.find(id) == before.end()) {
            added.push_back(fn);
        }
    }
    OD_ASSERT(added.size() == 1, 11,
              "turning Faster Song Playback on must add exactly one OnActorUpdate[ACTOR_PLAYER] registrant");
    const auto faster = added.front();

    auto songFrame = [&](u16 bgCheckFlags, f32 velocityY) {
        play->msgCtx.msgMode = MSGMODE_SONG_PLAYED;
        play->msgCtx.ocarinaAction = 0;
        play->msgCtx.stateTimer = 5;
        play->csCtx.state = 0;
        player->actor.bgCheckFlags = bgCheckFlags;
        player->actor.velocity.y = velocityY;
        faster(&player->actor);
        return play->msgCtx.stateTimer;
    };
    OD_ASSERT(songFrame(BGCHECKFLAG_GROUND, 0.0f) == 1, 12,
              "Faster Song Playback did not cut a song played on the ground (non-vacuity)");
    OD_ASSERT(songFrame(0, 2.0f) == 1, 13, "Faster Song Playback did not cut a song played while rising");
    OD_ASSERT(songFrame(0, -2.0f) == 5, 14, "Faster Song Playback cut a song played while falling (#1853)");
    printf("[TEST] mm-ocarina-dive: leg 4: Faster Song Playback leaves a falling song alone\n");

    MM_gPlayState = saved.play;
    return 0;
}

} // namespace

extern "C" int MM_OcarinaDive_RunHeadless(void) {
    Saved saved;
    saved.play = MM_gPlayState;
    memcpy(saved.waiting, sWaitingCutsceneList, sizeof(saved.waiting));
    memcpy(saved.next, sNextCutsceneList, sizeof(saved.next));
    saved.playbackState = MM_sPlaybackState;
    for (const char* key : kKeyedMainStartKeys) {
        saved.keys.emplace_back(key, CVarGetInteger(key, 0));
    }
    saved.keys.emplace_back(kFasterSongPlaybackKey, CVarGetInteger(kFasterSongPlaybackKey, 0));

    const int rc = Run(saved);
    Restore(saved);
    if (rc == 0) {
        printf("[TEST] mm-ocarina-dive: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
