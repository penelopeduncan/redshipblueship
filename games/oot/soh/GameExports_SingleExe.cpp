/**
 * Game Entry Points for OoT (Ship of Harkinian) - Single Executable Build
 *
 * This file provides the OoT_Game_* functions expected by the redship
 * main.cpp for single-executable builds.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <memory> // std::make_unique: the save format 6 fixture halves
#include <string>
#include "OTRGlobals.h"
#include "soh/CrashHandlerExt.h"
#include <libultraship/bridge.h>
#include <ship/Context.h>
#include "z64save.h"
#include "soh/cvar_prefixes.h" // CVAR_ENHANCEMENT: the continue value OoT_Combo_ReviveDeadHealthForFreeze mirrors
#include "soh/Enhancements/enhancementTypes.h" // BunnyHoodMode: OoT_BunnyHoodEffectMode (#969)

#include "game_lifecycle.h"
#include "integration_test_hooks.h"
#include "context.h"
#include "save.h" // RsbsSave_SetActiveSlot — publish the slot MM will save into
#include "shared_items.h"
#include "shared_resources.h" // Shared cross-game rupees/hearts (#525)
#include "triforce_hunt.h"    // ADR 0010 O10: the one triforce piece count's apply cap
#include "foreign_items.h"    // OoT_ForeignItem_Give (Lane C1 redemption)
#include "entrance.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Notification/Notification.h" // Lane 6 (#494): the foreign-arrival toast
// Rando::Context's definition, for GetBombchuCapacity() in the ammo shim.
// OTRGlobals.h only forward-declares Rando::Context, which is enough to hold
// the shared_ptr but not to call through it.
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/SaveManager.h"       // SaveFileMetaInfo: the paired row's slot ownership mark
#include "notification_bridge.h"   // the paired row's "no refusal toast" check
#include "pairing_refusal_toast.h" // RSBS_REFUSAL_TOAST_PREFIX: that check's matcher (#836)
#include "crossing_store.h"        // the paired row's crossing-store check
#include "combo_song_pool.h"       // #1542: the song checks' give paths (int-song-host-oot)
#include "combo_logic.h"           // #1542: the song-host rows' pin (ComboLogicForcedPlacement)
#include "foreign_model.h"         // the #800 S1 shop playtest's model answer
// The paired row's "shipped defaults" check: every setting the generation
// reads, per surface (OoT's options/tricks/exclusions, MM's options/tricks, the
// combo settings), must be unset in the CVar store.
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/resource/type/Text.h" // #981: the cache-owner row's staff text type check
#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h"
#include "combo_settings_view.h"
#include "warp_song_soaring.h"      // int-soaring-to-owl (#1169 XW6): the owls, the text ids, the owl mask
#include "mm_warp_song_play.h"      // int-mm-warp-song-minuet (#1169 XW13): the record, the pads, the latch
#include "mm_warp_song_drive.h"     // int-mm-warp-song-minuet: the expected arrival
#include "warp_songs_mm.h"          // int-mm-warp-song-minuet: the ownership byte
#include "warp_song_items.h"        // int-mm-warp-song-minuet: why a generation refused
#include "ComboItemTrackerWindow.h" // ComboItemTrackerFlushLayoutSave: the exit writes an unsettled layout edit (#1349)

// int-soaring-to-owl (#1169 XW8): the owl map's pause states and its prompt text.
#include "warp_song_soaring_map.h"
// #750: OoT_RetireAbandonedSession resets the actor DB's clients and drops every
// per-actor object extension of the session a departure abandons.
#include "soh/ActorDB.h"
#include "soh/ObjectExtension/ObjectExtension.h"
// SET_NEXT_GAMESTATE for the gameplay round-trip driver. Must come after
// GameInteractor.h (-> z64.h): macros.h declares `extern GraphicsContext*`
// and needs the type defined first.
#include "macros.h"
// The #577 M5 chest playtest reads a chest actor's entry; the #800 S1 shop
// playtest restocks a bought shelf the way the shopkeeper does.
extern "C" {
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "src/overlays/actors/ovl_En_GirlA/z_en_girla.h"
}

// External declarations from main.c and other C sources
extern "C" {
void GameConsole_Init(void);
void InitOTR(int argc, char* argv[]);
void DeinitOTR(void);
void OoT_Heaps_Alloc(void);
void OoT_Heaps_Free(void);
void Main(void* arg);
void BootCommands_Init(void);

// Audio cleanup for suspend (issue #160)
void OoT_Audio_PreNMI(void);
// Retire the graph coroutine on suspend (games/oot/src/code/graph.c) —
// re-entry after a switch re-inits the system arena under any suspended
// gamestate, so the frame loop must cold-start instead of resuming.
void OoT_Graph_ResetRunFrameContext(void);
// Defined below OoT_Game_Suspend, which calls it (#750).
void OoT_RetireAbandonedSession(void);
// Wait for the OTR audio std::thread to finish any in-flight buffer before
// the switch hot-swaps resource archives (OTRGlobals.cpp).
void OoT_Audio_DrainForSuspend(void);
extern s32 gAudioContextInitalized;
void Audio_InitMesgQueues(void);
// Restart the sound system on resume (code_800EC960.c) — suspend's
// PreNMI halts the sequence players, and OoT_AudioMgr_Init's bring-up
// (OoT_Audio_Init + OoT_Audio_InitSound) is behind a static
// hasInitialized guard that never re-runs (audioMgr.c).
void OoT_Audio_InitSound(void);
// Clears the PreNMI resetTimer latch (code_800E4FE0.c) — while it is
// nonzero every sequence start is silently dropped.
void OoT_Audio_ResumeFromPreNMI(void);

// OoT's SaveContext (type from z64save.h).
// Declared here so OoT_Game_Resume() can restore it on return from MM (#170).
extern SaveContext gSaveContext;
}

// The cross-game shadow buffers and unified gSaveContext storage are sized at
// OOT_SAVE_CONTEXT_SIZE (src/common/game.h), which src/common code cannot
// derive from sizeof(SaveContext) because it never includes z64save.h. This TU
// can, so it enforces the capacity here: if SoH's ship.* extension grows past
// the capacity, the build fails instead of freeze/restore silently truncating
// OoT save state on every cross-game switch.
static_assert(sizeof(SaveContext) <= OOT_SAVE_CONTEXT_SIZE,
              "OOT_SAVE_CONTEXT_SIZE (src/common/game.h) is smaller than SoH's runtime SaveContext; "
              "raise the capacity or cross-game freeze/restore will truncate save state");

// ---- Save format 6 (src/common/save.h; context.h's growth rule) --------------
// THE CAPACITY LOCKS. Every array OoT saves that upstream sizes by a growing enum
// has a fixed capacity in z64save.h; the enum must fit it. Outgrowing one is a
// save format generation, never a silent truncation.
static_assert((unsigned)TIMESTAMP_MAX <= RSBS_OOT_ITEM_TIMESTAMP_CAP &&
                  sizeof(((SohStats*)0)->itemTimestamp) == RSBS_OOT_ITEM_TIMESTAMP_CAP * sizeof(u32),
              "OoT's TIMESTAMP_MAX outgrew SohStats.itemTimestamp's saved capacity: a save format generation");
static_assert((unsigned)COUNT_MAX <= RSBS_OOT_STATS_COUNT_CAP &&
                  sizeof(((SohStats*)0)->count) == RSBS_OOT_STATS_COUNT_CAP * sizeof(u32),
              "OoT's COUNT_MAX outgrew SohStats.count's saved capacity: a save format generation");
static_assert((unsigned)RAND_INF_MAX <= RSBS_OOT_RAND_INF_CAP &&
                  sizeof(((ShipSaveContextData*)0)->randomizerInf) == RSBS_OOT_RAND_INF_CAP / 16 * sizeof(u16),
              "OoT's RAND_INF_MAX outgrew ship.randomizerInf's saved capacity: a save format generation");
static_assert(sizeof(ShipRandomizerSaveContextData) == RSBS_OOT_RANDO_QUEST_DATA_SIZE &&
                  sizeof(ShipRandomizerSaveContextData) >= sizeof(ShipBossRushSaveContextData) &&
                  sizeof(ShipQuestSpecificSaveContextData) == RSBS_OOT_RANDO_QUEST_DATA_SIZE,
              "ShipRandomizerSaveContextData is exactly 48 bytes (its counters, then rsbsSpare) and is the "
              "quest union's largest member: carve rsbsSpare's front, never grow the struct");
static_assert((unsigned)RC_MAX <= 0xFFFFu && (unsigned)RAND_INF_MAX <= 0xFFFFu && (unsigned)RSK_MAX <= 0xFFFFu,
              "the save watermark (context.h ComboSaveWatermark) stamps these as u16");
// THE NAME-TO-NUMBER LOCKS for the existing tails: an id appended in the wrong
// place (or an enumerator removed) changes the number every save, spoiler and
// pinned digest knows the last id by, and fails here instead.
static_assert(RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER == 2527,
              "OoT's last check id at save format 6 is 2527: append new checks after it, never insert");
static_assert(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER == 2063,
              "OoT's last randomizer-inf flag at save format 6 is 2063: append after it, never insert");
static_assert(RSK_KEYRINGS_CHEST_GAME == 233,
              "OoT's last setting key at save format 6 is 233: append after it, never insert");
// Each backport appended after format 6 locks its own ids the same way, so a
// merge that reorders the appended blocks fails here instead of renumbering
// saved rows. #1407 (Shuffle Butterfly Fairies): 20 checks, their 20 flags, 14
// hint texts and one setting key.
static_assert(RC_HC_NEAR_WALL_BUTTERFLY_FAIRY == 2528 && RC_KAK_WATCHTOWER_BUTTERFLY_FAIRY == 2547,
              "OoT's butterfly fairy checks (#1407) are ids 2528..2547, appended after the shopkeeper's");
static_assert(RAND_INF_HC_NEAR_WALL_BUTTERFLY_FAIRY == 2064 && RAND_INF_KAK_WATCHTOWER_BUTTERFLY_FAIRY == 2083,
              "OoT's butterfly fairy flags (#1407) are 2064..2083, appended after the shopkeeper's");
static_assert(RHT_BUTTERFLY_FAIRY_HYRULE_CASTLE == 1517 && RHT_BUTTERFLY_FAIRY_KAKARIKO_VILLAGE == 1530,
              "OoT's butterfly fairy hint texts (#1407) are 1517..1530, appended after the shopkeeper's");
static_assert(RSK_SHUFFLE_BUTTERFLY_FAIRIES == 234,
              "OoT's Shuffle Butterfly Fairies key (#1407) is 234, after RSK_KEYRINGS_CHEST_GAME");
// #1363 (Shuffle Silver Rupees), after #1407's: 154 checks, their 154 flags, 14
// hint texts, 22 items and one setting key.
static_assert(RC_SHADOW_N_UNDER_BLADE_SILVER == 2548 && RC_GANONS_CASTLE_MQ_MOVING_PLATFORM_SHADOW_SILVER == 2701,
              "OoT's silver rupee checks (#1363) are ids 2548..2701, appended after the butterfly fairies'");
static_assert(RAND_INF_RC_SHADOW_N_UNDER_BLADE_SILVER == 2084 &&
                  RAND_INF_GANONS_CASTLE_MQ_MOVING_PLATFORM_SHADOW_SILVER == 2237,
              "OoT's silver rupee flags (#1363) are 2084..2237, appended after the butterfly fairies'");
static_assert(RHT_SILVER_RUPEE_DODONGOS_CAVERN == 1531 && RHT_GANONS_CASTLE_SILVER == 1544,
              "OoT's silver rupee hint texts (#1363) are 1531..1544, appended after the butterfly fairies'");
static_assert(RG_SHADOW_SILVER_BLADES == 307 && RG_GANONS_CASTLE_MQ_SILVER_SHADOW == 328,
              "OoT's silver rupee items (#1363) are 307..328, appended after RG_NUTS");
static_assert(RSK_SHUFFLE_SILVER == 235, "OoT's Shuffle Silver Rupees key (#1363) is 235, after #1407's");
// #1404 (Starting Items + Shuffle Zelda's Letter + Progressive BGS + Starting Masks),
// after #1363's: 44 setting keys (236..279); RG_PROGRESSIVE_GORONSWORD already existed.
static_assert(RSK_STARTING_MEGATON_HAMMER == 236 && RSK_STARTING_BUNNY_HOOD == 269 &&
                  RSK_SHUFFLE_ZELDAS_LETTER == 270 && RSK_PROGRESSIVE_GORON_SWORD == 272 &&
                  RSK_STARTING_KEATON_MASK == 273 && RSK_STARTING_MASK_OF_TRUTH == 279,
              "OoT's #1404 setting keys are 236..279, appended after RSK_SHUFFLE_SILVER");
// #1405 (Shuffle Wonder Items), after #1363's checks/flags/hints and #1404's setting keys:
// 201 checks, their 201 flags, 26 hint texts and one setting key.
static_assert(RC_KF_WONDER_TRAINING_1 == 2702 && RC_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL == 2902,
              "OoT's wonder item checks (#1405) are ids 2702..2902, appended after the silver rupees'");
static_assert(RAND_INF_KF_WONDER_TRAINING_1 == 2238 &&
                  RAND_INF_GANONS_CASTLE_MQ_WONDER_SHADOW_TRIAL == 2438,
              "OoT's wonder item flags (#1405) are 2238..2438, appended after the silver rupees'");
static_assert(RHT_WONDER_ITEM_KOKIRI_FOREST == 1545 && RHT_GANONS_CASTLE_WONDER_ITEM == 1570,
              "OoT's wonder item hint texts (#1405) are 1545..1570, appended after the silver rupees'");
static_assert(RSK_SHUFFLE_WONDER_ITEMS == 280, "OoT's Shuffle Wonder Items key (#1405) is 280, after #1404's");
static_assert(RT_GTG_STATUE_JUMP == RT_DMT_SHIELDLESS_CLIMB + 1,
              "OoT's RT_GTG_STATUE_JUMP (#1405) is appended after RT_DMT_SHIELDLESS_CLIMB");
// #1363's carve (context.h growth rule 2, layout revision 3): the 22 one-byte silver
// counters take bytes 2..23 of the 48-byte randomizer arm, and rsbsSpare starts at
// 24. Every member is one byte, so these offsets hold on every compiler.
static_assert(offsetof(ShipRandomizerSaveContextData, silverShadowBlades) == 2 &&
                  offsetof(ShipRandomizerSaveContextData, silverMqGanonShadow) == 23 &&
                  offsetof(ShipRandomizerSaveContextData, rsbsSpare) == 24 &&
                  sizeof(((ShipRandomizerSaveContextData*)0)->rsbsSpare) == RSBS_OOT_RANDO_QUEST_DATA_SIZE - 24,
              "#1363's silver counters are carved from the front of rsbsSpare: bytes 2..23, the spare from 24");
// THE LAYOUT PINS (src/common/save_layout_pins.h): 0 until measured, then exact.
RSBS_LAYOUT_PIN_ASSERT(RSBS_PIN_OOT_SAVE_CONTEXT_SIZEOF, sizeof(SaveContext));
RSBS_LAYOUT_PIN_ASSERT(RSBS_PIN_OOT_SHIP, offsetof(SaveContext, ship));
RSBS_LAYOUT_PIN_ASSERT(RSBS_PIN_OOT_SHIP_QUEST, offsetof(SaveContext, ship.quest));
RSBS_LAYOUT_PIN_ASSERT(RSBS_PIN_OOT_SHIP_FILENAME_LANGUAGE, offsetof(SaveContext, ship.filenameLanguage));
RSBS_LAYOUT_PIN_ASSERT(RSBS_PIN_OOT_SHIP_RANDOMIZER_INF, offsetof(SaveContext, ship.randomizerInf));
RSBS_LAYOUT_PIN_ASSERT(RSBS_PIN_OOT_SHIP_FOREIGN_MASK, offsetof(SaveContext, ship.foreignMask));

/** Save format 6: the bytes of OoT's SaveContext, the start of Tier-2's zero tail
 *  (save.cpp's tail check; MM's twin is MM_SeedPackage_HalfSize). */
extern "C" uint32_t OoT_SaveLayout_ContextSize(void) {
    return (uint32_t)sizeof(SaveContext);
}

/** Save format 6: OoT's terms of the save watermark (context.h ComboSaveWatermark). */
extern "C" void OoT_SaveLayout_EnumMaxima(uint16_t* outRcMax, uint16_t* outRandInfMax, uint16_t* outRskMax) {
    if (outRcMax != nullptr) {
        *outRcMax = (uint16_t)RC_MAX;
    }
    if (outRandInfMax != nullptr) {
        *outRandInfMax = (uint16_t)RAND_INF_MAX;
    }
    if (outRskMax != nullptr) {
        *outRskMax = (uint16_t)RSK_MAX;
    }
}

/** Save format 6: the SaveLayoutPins row's OoT lines (save_layout_pins.h); returns
 *  the number of nonzero pins that differ from the compiled value. */
extern "C" int OoT_SaveLayout_ReportPins(void) {
    int mismatches = 0;
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_OOT_SAVE_CONTEXT_SIZEOF, sizeof(SaveContext), mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_OOT_SHIP, offsetof(SaveContext, ship), mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_OOT_SHIP_QUEST, offsetof(SaveContext, ship.quest), mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_OOT_SHIP_FILENAME_LANGUAGE, offsetof(SaveContext, ship.filenameLanguage),
                           mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_OOT_SHIP_RANDOMIZER_INF, offsetof(SaveContext, ship.randomizerInf), mismatches);
    RSBS_LAYOUT_PIN_REPORT(RSBS_PIN_OOT_SHIP_FOREIGN_MASK, offsetof(SaveContext, ship.foreignMask), mismatches);
    return mismatches;
}

/**
 * Save format 6: THE FIXTURE'S OoT HALF (SaveFormatV6Fixture, the committed
 * tests/fixtures/save-format-6/redship_slot0.redsave). The writer sets these
 * fields BY NAME in a zeroed SaveContext; the check reads them back by name from
 * a loaded half. Indices are LITERALS, the last ids at save format 6 (count 105,
 * itemTimestamp 239, randomizerInf word 128 holds flag 2063): a later build's
 * enums grow, the fixture's bytes do not. A field that moves in a later build
 * reads other bytes and fails by name, which is the per-step proof that no saved
 * field moved.
 */
extern "C" void OoT_SaveLayout_FixtureWrite(uint8_t* blob, size_t size) {
    if (blob == nullptr || size < sizeof(SaveContext)) {
        return;
    }
    auto sc = std::make_unique<SaveContext>();
    memset(sc.get(), 0, sizeof(SaveContext));
    sc->healthCapacity = 0x0130;
    sc->ship.stats.itemTimestamp[0] = 0x01020304u;
    sc->ship.stats.itemTimestamp[239] = 0x0A0B0C0Du;
    sc->ship.stats.count[0] = 0x11u;
    sc->ship.stats.count[105] = 0x22u;
    sc->ship.quest.id = QUEST_RANDOMIZER;
    sc->ship.quest.data.randomizer.triforcePiecesCollected = 7;
    sc->ship.quest.data.randomizer.bombchuUpgradeLevel = 2;
    sc->ship.filenameLanguage = 1;
    sc->ship.randomizerInf[0] = 0x1234u;
    sc->ship.randomizerInf[128] = 0x8001u;
    sc->ship.foreignMask = 3;
    memcpy(blob, sc.get(), sizeof(SaveContext));
}

extern "C" int OoT_SaveLayout_FixtureCheck(const uint8_t* blob, size_t size) {
    if (blob == nullptr || size < sizeof(SaveContext)) {
        printf("[FIXTURE] FAIL oot: no half to read (%zu bytes)\n", size);
        return 1;
    }
    auto sc = std::make_unique<SaveContext>();
    memcpy(sc.get(), blob, sizeof(SaveContext));
    int bad = 0;
    RSBS_LAYOUT_FIXTURE_FIELD("oot.healthCapacity", sc->healthCapacity, 0x0130, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.stats.itemTimestamp[0]", sc->ship.stats.itemTimestamp[0], 0x01020304u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.stats.itemTimestamp[239]", sc->ship.stats.itemTimestamp[239], 0x0A0B0C0Du, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.stats.count[0]", sc->ship.stats.count[0], 0x11u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.stats.count[105]", sc->ship.stats.count[105], 0x22u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.quest.id", sc->ship.quest.id, QUEST_RANDOMIZER, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.quest.data.randomizer.triforcePiecesCollected",
                              sc->ship.quest.data.randomizer.triforcePiecesCollected, 7, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.quest.data.randomizer.bombchuUpgradeLevel",
                              sc->ship.quest.data.randomizer.bombchuUpgradeLevel, 2, bad);
    // #1363's carve (layout revision 3): an earlier file's spare bytes were zero, so
    // the silver counters carved from them read 0, none collected. A carve that
    // moved bombchuUpgradeLevel or wrote over it would fail the line above instead.
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.quest.data.randomizer.silverShadowBlades",
                              sc->ship.quest.data.randomizer.silverShadowBlades, 0, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.quest.data.randomizer.silverMqGanonShadow",
                              sc->ship.quest.data.randomizer.silverMqGanonShadow, 0, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.filenameLanguage", sc->ship.filenameLanguage, 1, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.randomizerInf[0]", sc->ship.randomizerInf[0], 0x1234u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.randomizerInf[128]", sc->ship.randomizerInf[128], 0x8001u, bad);
    RSBS_LAYOUT_FIXTURE_FIELD("oot.ship.foreignMask", sc->ship.foreignMask, 3, bad);
    return bad;
}

// #1176: SoH appended skeletonHeader to OoT's SkelAnime; MM's has no such
// field. The skeleton-patcher-mm-layout row checks this size is MM's
// (MM_SkelAnime_SizeOf) plus that one trailing pointer, so the layouts cannot
// drift apart silently; `skeleton` sits at the same offset in both.
static_assert(offsetof(SkelAnime, skeletonHeader) == sizeof(SkelAnime) - sizeof(void*),
              "OoT SkelAnime::skeletonHeader is no longer the trailing field");
static_assert(offsetof(SkelAnime, skeleton) == sizeof(void*), "OoT SkelAnime::skeleton moved");
extern "C" size_t OoT_SkelAnime_SizeOf(void) {
    return sizeof(SkelAnime);
}

// Archive hot-swap cycle helpers (#263). Defined in
// src/common/tests/test_archive_hotswap.c (compiled into redship_common via
// test_runner.cpp); resolved at final link. Record this OoT arrival, query the
// RSS bound, and read the target arrival count for the cycle-complete check.
extern "C" {
int ArchiveHotswap_RecordArrival(void);
int ArchiveHotswap_RssExceeded(void);
int ArchiveHotswap_TargetArrivals(void);
// #981 resource cache owner checks (src/common/tests/test_resource_cache_owner.c).
int ResourceCacheOwner_CheckArrival(GameId game, int arrival, const void* skel, int limbCount, char* msg, size_t cap);
int ResourceCacheOwner_MMLegPassed(void);
int ResourceCacheOwner_PatchProbeOoTDepart(char* msg, size_t cap); // #1040 R2
int ResourceCacheOwner_PatchProbeOoTReturn(char* msg, size_t cap);
int ResourceCacheOwner_CheckBootOwner(const char* when, char* msg, size_t cap); // #1040 R3
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
}

// Game state
static int sArgc = 0;
static char** sArgv = nullptr;

// Integration test hook frame counter (reset each time hooks are registered)
static int sOoTGameStateMainFrameCount = 0;

// ============================================================================
// Gameplay round-trip repro (INT_TEST_GAMEPLAY_ROUNDTRIP) — OoT side
// ============================================================================

// Symbols the gameplay driver borrows from OoT proper. All C linkage:
// OoT_Sram_InitDebugSave / OoT_Play_Init are decomp code (z_sram.c/z_play.c),
// OoT_gGameState / OoT_gPlayState are set by game.c / z_play.c.
extern "C" {
void OoT_Sram_InitDebugSave(void);
void OoT_Play_Init(GameState* thisx);
extern GameState* OoT_gGameState;
extern PlayState* OoT_gPlayState;
// Camera constant registers live in gGameInfo, which Main() re-mints (zeroed)
// on every OoT entry; the seeding is re-armed from func_800636C0. See
// games/oot/src/code/z_camera.c.
bool OoT_Camera_RegsSeeded(void);
}

// Phase-local OoT driver state. sGpArrivalPhase records which phase's
// expected scene has been confirmed by OnSceneInit; gameplay frames are only
// counted while the arrival matches the live phase, so fade-out frames after
// firing a door and pre-arrival frames never count.
static GameplayPhase sGpArrivalPhase = GP_PHASE_DONE;
static GameplayPhase sGpPlayerLastPhase = GP_PHASE_DONE;
static int sGpFramesInPhase = 0;
static int sGpSceneInits = 0;
// Wall-clock phase watchdog (#376 item 4). The budget is seconds, not frames:
// a frame budget could not fire before the CTest/`timeout` wall clock under
// llvmpipe, so the diagnostic dump never emitted. The timer's decision is
// IntegrationTest_GameplayWatchdogTick: a phase change or a completed gameplay
// frame (sGpFramesInPhase advancing) is progress (#882), a phase that makes no
// progress for sGpWatchdogBudgetSecs fails the run with state, and one that
// keeps advancing past the hard ceiling (four budgets) fails too.
static GameplayWatchdogTimer sGpWatchdogTimer = { (int)GP_PHASE_DONE, 0, 0.0, 0.0 };
static int sGpWatchdogBudgetSecs = 0;

static double GpWatchdogNowSecs(void) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static bool sGpWatchdogFired = false;
// Door-actor presence check (bug 1a): baseline door count captured in the
// boot-phase scene, compared on the return leg when the scene matches. -1 =
// no baseline captured.
static int sGpDoorBaseline = -1;
static int16_t sGpDoorBaselineScene = -1;
// Camera-follow assert (bug 1b): snapshot of the active camera + player taken
// once the arrival settles; the driver then force-marches the player and
// fails if the camera's eye+at never move while the player does.
static Vec3f sGpCamStartEye;
static Vec3f sGpCamStartAt;
static Vec3f sGpCamStartPlayer;
static int sGpCamProbeArmed = 0;

static float GpVecDist(const Vec3f* a, const Vec3f* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * The production "walk through a door" trio (z_player.c exit handling /
 * debugconsole `entrance` command): the play update loop consumes it on the
 * next frame, runs the real transition state machine — including
 * Combo_CheckEntranceSwitch on the cross-game path, which freezes the live
 * SaveContext — and re-enters OoT_Play_Init for same-game targets.
 */
// #380: the env-side pre-filter in src/common/integration_test_hooks.cpp keeps a
// literal copy of this bound (kOoTEntranceMax) because src/common cannot include
// OoT headers. Lock the two at compile time so that literal cannot silently drift
// out of sync with the real gEntranceTable size.
static_assert(ENTR_MAX == 0x0614, "ENTR_MAX moved; sync kOoTEntranceMax in integration_test_hooks.cpp");

static void GpFireOoTDoor(uint16_t entrance, const char* what) {
    PlayState* play = OoT_gPlayState;
    if (play == NULL) {
        IntegrationTest_GameplayFail("no PlayState when firing a door transition");
        return;
    }
    // #380: nextEntranceIndex is consumed as a raw linear index into
    // gEntranceTable[ENTR_MAX] (games/oot/include/variables.h) with no bound
    // check downstream. This is the consumption-time re-check the env filter's
    // comment promises: an out-of-range id (e.g. RSBS_GP_WARP_ENTRANCE against a
    // shrunken table) fails the test loudly instead of reading gEntranceTable out
    // of bounds — the OOB-read crash class Test_StartupEntrance guards.
    if (entrance >= ENTR_MAX) {
        fprintf(stderr, "[GP-TEST] refusing %s: entrance 0x%04X is out of range (ENTR_MAX 0x%04X)\n", what, entrance,
                (unsigned)ENTR_MAX);
        fflush(stderr);
        IntegrationTest_GameplayFail("door-transition entrance index >= ENTR_MAX");
        return;
    }
    fprintf(stderr, "[GP-TEST] firing %s: entrance 0x%04X (from scene %d, entrance 0x%04X)\n", what, entrance,
            play->sceneNum, (uint16_t)gSaveContext.entranceIndex);
    fflush(stderr);
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
}

/**
 * Author a debug save in-place and enter Play directly at `entrance`. Mirrors
 * the operator's map-select flow — Select_LoadGame (z_select.c) and the boot
 * branch of Enhancements/Warping.cpp Warp() — so the test runs on the same
 * kind of full-inventory save + Play_Init entry the manual repro uses.
 *
 * Shared by every integration row that needs live gameplay without a Start
 * press: called from the title screen's OnZTitleUpdate (which ticks every
 * frame of the boot logo, unattended) or, belt-and-braces, from
 * OnPresentFileSelect. `tag` prefixes the log line. Returns false (and does
 * nothing) when there is no GameState to redirect.
 */
static bool IntInjectDebugSaveAndEnterPlay(GameState* gameState, uint16_t entrance, bool adult, const char* tag,
                                           const char* from) {
    if (gameState == NULL) {
        return false;
    }
    fprintf(stderr, "[%s] injecting debug save at %s; entering Play at entrance 0x%04X\n", tag, from, entrance);
    fflush(stderr);

    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFE; // temporary file so InitDebugSave respects the debug-save-file option
    OoT_Sram_InitDebugSave();
    gSaveContext.magicFillTarget = gSaveContext.magic;
    gSaveContext.magic = 0;
    gSaveContext.magicCapacity = 0;
    gSaveContext.magicLevel = gSaveContext.magic;
    gSaveContext.fileNum = 0xFF;
    gSaveContext.sceneSetupIndex = 0;
    gSaveContext.cutsceneIndex = 0;
    // child + noon => the populated Market Day of the crash logs. With
    // RSBS_GP_BOOT_AGE=adult the save boots adult instead, so the run
    // exercises the forced-child-on-return swap in OoT_Game_Resume (the
    // return-leg assert requires child either way).
    gSaveContext.linkAge = adult ? LINK_AGE_ADULT : LINK_AGE_CHILD;
    gSaveContext.nightFlag = 0;
    gSaveContext.dayTime = 0x8000;
    gSaveContext.skyboxTime = 0x8000;
    gSaveContext.entranceIndex = entrance;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;

    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, OoT_Play_Init, PlayState);
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
    return true;
}

// The gameplay round-trip's boot step: the configured entrance and age.
static void GpInjectDebugSaveAndEnterPlay(GameState* gameState, const char* from) {
    const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
    if (!IntInjectDebugSaveAndEnterPlay(gameState, cfg->bootEntrance, cfg->bootAdult != 0, "GP-TEST", from)) {
        IntegrationTest_GameplayFail("no GameState available for debug-save injection");
        return;
    }
    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_PRE);
}

// ============================================================================
// T1 (#260, #544): Happy Mask Shop -> MM, fired from live gameplay
// ============================================================================
//
// #544: this row used to fire from OnPresentFileSelect, whose only dispatch is
// FileChoose_FinishFadeIn (z_file_choose.c) — reached only after a Start press
// at the title, which no unattended run sends. OoT settled into the attract
// demo loop (cutsceneIndex 0xfff3/0xfff2) until the CTest wall killed it, so
// the row never executed one of its assertions. It now reaches gameplay the
// way IntGameplayRoundtrip does — a debug save injected from the title screen,
// Play entered directly outside the Happy Mask Shop — fires the HMS entrance
// after a window of live gameplay frames, and, when an OoT stage before the
// trigger stalls while OoT keeps ticking frames (the #544 attract-demo state),
// fails with the stage it was stuck in on a per-stage wall-clock budget instead
// of hitting the wall silently.
//
// What this budget does NOT cover: it is checked from OoT's
// OnGameStateMainStart, so (a) a wedge inside one OoT frame (e.g. a hang in
// Play_Init after the injection) never reaches the check, and (b) once the row
// is T1_STAGE_TRIGGERED the check stops, and the OoT->MM hand-off (suspend,
// archive swap, MM_Game_Init) and the MM half have only MM's frame-counted
// scene-load watchdog. Those stalls, where no frame completes, are the
// wall-clock watchdog thread's (#793, src/common/integration_test_hooks.cpp):
// it names the stage and ends the run before the CTest timeout.
typedef enum {
    T1_STAGE_BOOT,          // waiting for the title screen (or file select) to inject the debug save
    T1_STAGE_ENTERING_PLAY, // debug save injected; waiting for OoT's scene init at kT1BootEntrance
    T1_STAGE_GAMEPLAY,      // scene built; counting live gameplay frames before the trigger
    T1_STAGE_TRIGGERED,     // HMS entrance fired and routing asserted; MM's side owns the rest
    T1_STAGE_FAILED,
} T1Stage;

static T1Stage sT1Stage = T1_STAGE_BOOT;
static int sT1GameplayFrames = 0;
static std::chrono::steady_clock::time_point sT1StageStart{};
// Right outside the Happy Mask Shop: where a player stands before walking in.
static const uint16_t kT1BootEntrance = OOT_ENTR_MARKET_FROM_MASK_SHOP;
static const int kT1GameplayFramesBeforeTrigger = 20;
// Per stage, wall clock, checked once per OoT frame. One stalled stage fails
// the row, so a stall is reported at about 30 s plus the time spent in earlier
// stages (a healthy run spends under a second in each). It bounds only OoT
// stages that keep ticking frames; see the block comment above for the stalls
// the wall-clock watchdog thread bounds instead.
static const int kT1StageBudgetSecs = 30;

static const char* T1StageName(T1Stage stage) {
    switch (stage) {
        case T1_STAGE_BOOT:
            return "boot";
        case T1_STAGE_ENTERING_PLAY:
            return "entering-play";
        case T1_STAGE_GAMEPLAY:
            return "gameplay";
        case T1_STAGE_TRIGGERED:
            return "triggered";
        case T1_STAGE_FAILED:
            return "failed";
    }
    return "unknown";
}

static double T1SecondsInStage(void) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - sT1StageStart).count();
}

static void T1SetStage(T1Stage stage) {
    fprintf(stderr, "[OoT-INT-TEST] T1 stage %s -> %s (%.1f s in %s)\n", T1StageName(sT1Stage), T1StageName(stage),
            T1SecondsInStage(), T1StageName(sT1Stage));
    fflush(stderr);
    sT1Stage = stage;
    sT1StageStart = std::chrono::steady_clock::now();
}

// Fail loudly with the stage and enough state to attribute the wedge from the
// log alone. RequestExit does not set the pass flag; Combo_RequestGameSwitch
// makes Combo_CheckHotSwap return true so OoT's frame loop actually hands back
// to main, which sees the exit request before any switch.
static void T1Fail(const char* why) {
    PlayState* play = OoT_gPlayState;
    fprintf(stderr,
            "[OoT-INT-TEST] FAIL (int-switch-oot-hms-to-mm): %s [stage=%s after %.1f s; gameMode=%d "
            "entranceIndex=0x%04X cutsceneIndex=0x%04X gPlayState=%p sceneNum=%d]\n",
            why, T1StageName(sT1Stage), T1SecondsInStage(), (int)gSaveContext.gameMode,
            (uint16_t)gSaveContext.entranceIndex, (uint16_t)gSaveContext.cutsceneIndex, (void*)play,
            play != NULL ? (int)play->sceneNum : -1);
    fflush(stderr);
    sT1Stage = T1_STAGE_FAILED;
    IntegrationTest_RequestExit();
    Combo_RequestGameSwitch();
}

static void T1InjectFrom(GameState* gameState, const char* from) {
    if (sT1Stage != T1_STAGE_BOOT) {
        return;
    }
    if (!IntInjectDebugSaveAndEnterPlay(gameState, kT1BootEntrance, false, "OoT-INT-TEST", from)) {
        T1Fail("no GameState available for debug-save injection");
        return;
    }
    T1SetStage(T1_STAGE_ENTERING_PLAY);
}

// #981 (int-resource-cache-owner): OoT's own copies of two paths MM's archive
// also carries, before the crossing and after the return. The first check
// records the objects; the second must be served the very same ones.
static const char* const kRcoOctorokSkel = "objects/object_okuta/gOctorokSkel";
static const char* const kRcoStaffText = "text/staff_message_data_static/staff_message_data_static";
static const void* sRcoOoTSkel = nullptr;
static const void* sRcoOoTText = nullptr;
static int sRcoReturnFrames = 0;

static bool RcoOoTCheck(const char* when, char* msg, size_t cap) {
    SkeletonHeader* skel = ResourceMgr_LoadSkeletonByName(kRcoOctorokSkel, NULL);
    auto text = std::dynamic_pointer_cast<SOH::Text>(
        Ship::Context::GetInstance()->GetResourceManager()->LoadResource(kRcoStaffText));
    const int limbs = skel != NULL ? skel->limbCount : -1;
    std::string fail;
    if (limbs != 37) {
        fail += "gOctorokSkel has " + std::to_string(limbs) + " limbs, expected OoT's 37; ";
    }
    if (text == nullptr) {
        fail += "the staff text is not a SOH::Text; ";
    }
    if (sRcoOoTSkel == nullptr) {
        sRcoOoTSkel = skel;
        sRcoOoTText = text.get();
    } else if (sRcoOoTSkel != skel || sRcoOoTText != text.get()) {
        char line[160];
        snprintf(line, sizeof(line), "served gOctorokSkel %p / staff text %p, not the %p / %p it had before; ",
                 (void*)skel, (void*)text.get(), sRcoOoTSkel, sRcoOoTText);
        fail += line;
    }
    if (!fail.empty()) {
        snprintf(msg, cap, "OoT %s: %s", when, fail.c_str());
        return false;
    }
    snprintf(msg, cap, "OoT %s: gOctorokSkel %p with 37 limbs, staff text %p is a SOH::Text", when, (void*)skel,
             (void*)text.get());
    return true;
}

// Fire the HMS entrance and assert where it routes. Same call OoT's z_play.c
// makes when the player walks into the Happy Mask Shop door — minus the
// SaveContext freeze, which IntGameplayRoundtrip's production door covers.
static void T1TriggerHmsAndAssertRouting(void) {
    if (IntegrationTest_GetMode() == INT_TEST_RESOURCE_CACHE_OWNER) {
        char msg[384];
        if (!RcoOoTCheck("before the crossing", msg, sizeof(msg))) {
            T1Fail(msg);
            return;
        }
        fprintf(stderr, "[OoT-INT-TEST] PASS cache-owner leg 1: %s\n", msg);
        // #1040 R2: a patch OoT makes before it leaves, for MM's leg to unpatch.
        if (!ResourceCacheOwner_PatchProbeOoTDepart(msg, sizeof(msg))) {
            T1Fail(msg);
            return;
        }
        fprintf(stderr, "[OoT-INT-TEST] PASS cache-owner leg 1 (patch registry): %s\n", msg);
    }
    fprintf(stderr, "[OoT-INT-TEST] gameplay reached (scene %d, %d live frames); triggering HMS entrance 0x%04X\n",
            OoT_gPlayState != NULL ? (int)OoT_gPlayState->sceneNum : -1, sT1GameplayFrames, OOT_ENTR_HAPPY_MASK_SHOP);
    fflush(stderr);

    Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);

    if (!Combo_IsCrossGameSwitch()) {
        T1Fail("HMS entrance did not register a cross-game switch");
        return;
    }

    const char* target = Combo_GetSwitchTargetGameId();
    uint16_t targetEntrance = Combo_GetSwitchTargetEntrance();

    if (!target || strcmp(target, "mm") != 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "target should be 'mm', got '%s'", target ? target : "(null)");
        T1Fail(msg);
        return;
    }

    if (targetEntrance != MM_ENTR_SOUTH_CLOCK_TOWN_0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "target entrance should be 0x%04X (South Clock Town tower exit), got 0x%04X",
                 MM_ENTR_SOUTH_CLOCK_TOWN_0, targetEntrance);
        T1Fail(msg);
        return;
    }

    fprintf(stderr, "[OoT-INT-TEST] PASS leg 1: HMS routes to MM 0x%04X; main loop will run the switch\n",
            targetEntrance);
    fflush(stderr);
    T1SetStage(T1_STAGE_TRIGGERED);
    // Intentionally NOT signaling boot complete here. OoT's frame loop sees
    // the pending cross-game switch in Combo_CheckHotSwap and hands off to MM;
    // the MM-side hook signals the final pass once South Clock Town is loaded
    // and stable (and has its own scene-load watchdog).
}

// ============================================================================
// int-crossing-refused (#954): a refused crossing leaves OoT running
// ============================================================================
//
// The row's script (CMake/CheckCrossingRefused.cmake) sets mm.o2r or 2ship.o2r
// aside, so no switch into Majora's Mask can go ahead. Both legs start from
// live gameplay outside the Happy Mask Shop on the debug save:
//
//   1. THE DOOR. The production door transition into 0x0530: z_play.c runs
//      Combo_CheckEntranceSwitch, which must refuse the crossing while OoT is
//      still in its frame loop. The scene then reloads at 0x01D1, outside the
//      door, with no switch pending, no departure freeze and OoT_Game_Run
//      entered once, and OoT plays on. Before #954 the loop exited with the
//      switch pending, main refused it after the fact and ran OoT_Game_Run
//      again: Main() re-initialized the system arena under the gamestate the
//      graph coroutine still pointed at, and the first frame read the
//      GraphicsContext out of freed memory (OoT_GameState_SetFrameBuffer).
//   2. A LATE REFUSAL. A raw switch request (Combo_RequestGameSwitch, which
//      MM's owl-save and game-over exits and these hooks call) leaves the loop
//      past the in-game gates. main must refuse it and bring OoT back through
//      its suspend/resume contract (GameRunner_ReenterActive), never a bare
//      second Main(): OoT comes back at 0x01D1 and plays on.
//
// "Plays on" is kCrLiveFrames live gameplay frames AND kCrLiveSecs of wall
// clock. A stage that stalls while OoT keeps ticking frames fails on
// kCrStageBudgetSecs; a dead or wedged process is the wall-clock watchdog
// thread's and the script's to report.
typedef enum {
    CR_STAGE_BOOT,          // waiting for the title screen to inject the debug save
    CR_STAGE_ENTERING_PLAY, // debug save injected; waiting for the scene init at kCrOutsideDoor
    CR_STAGE_BEFORE_DOOR,   // live frames outside the door
    CR_STAGE_DOOR,          // walked into the door; waiting for the scene init it leads to
    CR_STAGE_AFTER_DOOR,    // playing on after the refused door
    CR_STAGE_LATE,          // raw switch request made; waiting for OoT to come back
    CR_STAGE_AFTER_LATE,    // playing on after the late refusal
    CR_STAGE_DONE,
    CR_STAGE_FAILED,
} CrStage;

// How many times OoT_Game_Run has been entered in this process: once at boot,
// and once more for each time OoT's frame loop was left and re-entered.
static int sOoTRunEntries = 0;

static CrStage sCrStage = CR_STAGE_BOOT;
static int sCrLiveFrames = 0;
static std::chrono::steady_clock::time_point sCrStageStart{};
static const uint16_t kCrOutsideDoor = OOT_ENTR_MARKET_FROM_MASK_SHOP;
static const int kCrFramesBeforeDoor = 20;
static const int kCrLiveFrames = 60;
static const double kCrLiveSecs = 3.0;
static const int kCrStageBudgetSecs = 30;

static const char* CrStageName(CrStage stage) {
    switch (stage) {
        case CR_STAGE_BOOT:
            return "boot";
        case CR_STAGE_ENTERING_PLAY:
            return "entering-play";
        case CR_STAGE_BEFORE_DOOR:
            return "before-door";
        case CR_STAGE_DOOR:
            return "door";
        case CR_STAGE_AFTER_DOOR:
            return "after-door";
        case CR_STAGE_LATE:
            return "late";
        case CR_STAGE_AFTER_LATE:
            return "after-late";
        case CR_STAGE_DONE:
            return "done";
        case CR_STAGE_FAILED:
            return "failed";
    }
    return "unknown";
}

static double CrSecondsInStage(void) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - sCrStageStart).count();
}

static void CrSetStage(CrStage stage) {
    fprintf(stderr, "[OoT-INT-TEST] crossing-refused: stage %s -> %s (%.1f s, %d live frames in %s)\n",
            CrStageName(sCrStage), CrStageName(stage), CrSecondsInStage(), sCrLiveFrames, CrStageName(sCrStage));
    fflush(stderr);
    sCrStage = stage;
    sCrLiveFrames = 0;
    sCrStageStart = std::chrono::steady_clock::now();
}

// Same exit as T1Fail: RequestExit leaves the pass flag unset, and the switch
// request is what hands control back to main, which sees the exit first.
static void CrFail(const char* why) {
    PlayState* play = OoT_gPlayState;
    fprintf(stderr,
            "[OoT-INT-TEST] FAIL (int-crossing-refused): %s [stage=%s after %.1f s; OoT_Game_Run entries=%d "
            "gameMode=%d entranceIndex=0x%04X gPlayState=%p sceneNum=%d]\n",
            why, CrStageName(sCrStage), CrSecondsInStage(), sOoTRunEntries, (int)gSaveContext.gameMode,
            (uint16_t)gSaveContext.entranceIndex, (void*)play, play != NULL ? (int)play->sceneNum : -1);
    fflush(stderr);
    sCrStage = CR_STAGE_FAILED;
    IntegrationTest_RequestExit();
    Combo_RequestGameSwitch();
}

static void CrInjectFrom(GameState* gameState, const char* from) {
    if (sCrStage != CR_STAGE_BOOT) {
        return;
    }
    if (!IntInjectDebugSaveAndEnterPlay(gameState, kCrOutsideDoor, false, "OoT-INT-TEST", from)) {
        CrFail("no GameState available for debug-save injection");
        return;
    }
    CrSetStage(CR_STAGE_ENTERING_PLAY);
}

// The production "walk into the door" trio (GpFireOoTDoor): the play update
// loop runs the real transition, and z_play.c calls Combo_CheckEntranceSwitch.
static void CrWalkIntoDoor(void) {
    PlayState* play = OoT_gPlayState;
    fprintf(stderr,
            "[OoT-INT-TEST] crossing-refused: %d live frames outside the Happy Mask Shop; walking into its door "
            "(entrance 0x%04X)\n",
            sCrLiveFrames, OOT_ENTR_HAPPY_MASK_SHOP);
    fflush(stderr);
    CrSetStage(CR_STAGE_DOOR);
    play->nextEntranceIndex = OOT_ENTR_HAPPY_MASK_SHOP;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
}

static void CrRequestLateSwitch(void) {
    fprintf(stderr,
            "[OoT-INT-TEST] crossing-refused: leg 1 PASS (the door was refused in OoT's loop; %d live frames, %.1f s "
            "outside it); leg 2: a raw switch request leaves the loop past the in-game gates\n",
            sCrLiveFrames, CrSecondsInStage());
    fflush(stderr);
    CrSetStage(CR_STAGE_LATE);
    Combo_RequestGameSwitch();
}

// Gameplay (re)built: where the refused door, or the re-entry after the late
// refusal, put Link. Fires after the startup-entrance consumption, so
// entranceIndex is final here.
static void CrSceneInit(int16_t sceneNum) {
    if (Context_GetCurrentGame() != GAME_OOT) {
        return; // shared hook storage guard (#344)
    }
    const uint16_t entrance = (uint16_t)gSaveContext.entranceIndex;
    char msg[192];
    switch (sCrStage) {
        case CR_STAGE_ENTERING_PLAY:
            if (entrance != kCrOutsideDoor) {
                snprintf(msg, sizeof(msg), "gameplay arrived at entrance 0x%04X, expected 0x%04X", entrance,
                         kCrOutsideDoor);
                CrFail(msg);
                return;
            }
            CrSetStage(CR_STAGE_BEFORE_DOOR);
            return;
        case CR_STAGE_DOOR:
        case CR_STAGE_LATE: {
            const bool door = sCrStage == CR_STAGE_DOOR;
            fprintf(stderr,
                    "[OoT-INT-TEST] crossing-refused: scene init after the %s: scene %d, entrance 0x%04X, "
                    "OoT_Game_Run entries %d\n",
                    door ? "door" : "late refusal", sceneNum, entrance, sOoTRunEntries);
            fflush(stderr);
            if (entrance != kCrOutsideDoor) {
                snprintf(msg, sizeof(msg),
                         "after the %s, entrance 0x%04X loaded; a refused crossing must leave Link outside the "
                         "door (0x%04X)",
                         door ? "refused door" : "late refusal", entrance, kCrOutsideDoor);
                CrFail(msg);
                return;
            }
            if (Combo_IsCrossGameSwitch() || Combo_IsGameSwitchRequested()) {
                CrFail("a switch is still pending after the refusal");
                return;
            }
            if (door && Context_HasFrozenState(GAME_OOT)) {
                CrFail("the refused door froze OoT's half: the refusal must come before the departure freeze");
                return;
            }
            // #1040 R3: a refused crossing never reaches ApplyResourceCacheOwner,
            // so the booted game keeps owner 0.
            if (!ResourceCacheOwner_CheckBootOwner(door ? "after the refused door" : "after the late refusal", msg,
                                                   sizeof(msg))) {
                CrFail(msg);
                return;
            }
            const int expectedEntries = door ? 1 : 2;
            if (sOoTRunEntries != expectedEntries) {
                snprintf(msg, sizeof(msg),
                         door ? "OoT_Game_Run was entered %d times: OoT left its frame loop for the refused door "
                                "instead of being refused while it ran"
                              : "OoT_Game_Run was entered %d times, expected 2 (boot, then the re-entry after the "
                                "late refusal)",
                         sOoTRunEntries);
                CrFail(msg);
                return;
            }
            CrSetStage(door ? CR_STAGE_AFTER_DOOR : CR_STAGE_AFTER_LATE);
            return;
        }
        default:
            return;
    }
}

static void CrFrame(void) {
    if (Context_GetCurrentGame() != GAME_OOT) {
        return; // (#344) MM frames share this hook storage
    }
    if (sCrStage == CR_STAGE_DONE || sCrStage == CR_STAGE_FAILED) {
        return;
    }
    if (CrSecondsInStage() >= kCrStageBudgetSecs) {
        char msg[96];
        snprintf(msg, sizeof(msg), "stage %s made no progress for %d s", CrStageName(sCrStage), kCrStageBudgetSecs);
        CrFail(msg);
        return;
    }
    if (sCrStage != CR_STAGE_BEFORE_DOOR && sCrStage != CR_STAGE_AFTER_DOOR && sCrStage != CR_STAGE_AFTER_LATE) {
        return;
    }
    PlayState* play = OoT_gPlayState;
    if (play == NULL || OoT_gGameState != &play->state || gSaveContext.gameMode != GAMEMODE_NORMAL ||
        GET_PLAYER(play) == NULL) {
        return;
    }
    sCrLiveFrames++;
    if (sCrStage == CR_STAGE_BEFORE_DOOR) {
        if (sCrLiveFrames >= kCrFramesBeforeDoor) {
            CrWalkIntoDoor();
        }
        return;
    }
    if (sCrLiveFrames < kCrLiveFrames || CrSecondsInStage() < kCrLiveSecs) {
        return;
    }
    if (sCrStage == CR_STAGE_AFTER_DOOR) {
        CrRequestLateSwitch();
        return;
    }
    fprintf(stderr,
            "[OoT-INT-TEST] int-crossing-refused: PASS (the door was refused while OoT ran, the late refusal "
            "re-entered OoT through suspend/resume, and OoT played on after both: %d live frames, %.1f s)\n",
            sCrLiveFrames, CrSecondsInStage());
    fflush(stderr);
    CrSetStage(CR_STAGE_DONE);
    IntegrationTest_SignalBootComplete(GAME_OOT, "crossing refused; OoT played on");
}

static void CrRegisterHooks(void) {
    fprintf(stderr, "[OoT] Registering integration test hooks for the refused crossing (#954)\n");
    fflush(stderr);
    sCrStage = CR_STAGE_BOOT;
    sCrLiveFrames = 0;
    sCrStageStart = std::chrono::steady_clock::now();
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleUpdate>(
        [](void* gameState) { CrInjectFrom((GameState*)gameState, "title screen"); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>(
        []() { CrInjectFrom(OoT_gGameState, "file select"); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](int16_t sceneNum) { CrSceneInit(sceneNum); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() { CrFrame(); });
}

// ============================================================================
// int-paired-first-crossing (the gameplay round trip's paired variant)
// ============================================================================

extern "C" {
void OoT_Sram_InitSave(FileChooseContext* fileChooseCtx);
void OoT_Sram_OpenSave(void);
// ovl_file_choose: the file select gamestate (SoH's Boot Sequence: File Select target).
void FileChoose_Init(GameState* thisx);
u32 Save_Exist(int fileNum);
void Save_DeleteFile(int fileNum);
SaveFileMetaInfo* Save_GetSaveMetaInfo(int fileNum);
// 3drando/menu.cpp: the synchronous generation the rando-tier creation rows use.
int Rando_HeadlessSeedTest(const char* seedStr);
// OTRGlobals.h declares these inside its C-only block.
uint32_t Randomizer_GetCurrentWorldSeed(void);
// games/mm/2s2h/GameExports_SingleExe.cpp: MM's generation dispatch count.
uint32_t MM_Rando_OnSaveInitDispatchCount(void);
// soh/SaveManager.cpp: the file select's gate (#836).
int OoT_FileSelect_ProbeSlotForOpen(int fileNum, char* words, size_t wordsLen);
}

// The slot the paired row writes: the THIRD file (fileNum 2), named "RSBSTEST"
// in OoT's NTSC filename charset. The name is also the row's ownership mark: a
// slot holding a file by any other name is refused, never erased, so running the
// row in a build directory somebody plays in cannot destroy their save.
static const int kPfcSlot = 2;
static const uint8_t kPfcName[8] = { 0xBC, 0xBD, 0xAC, 0xBD, 0xBE, 0xAF, 0xBD, 0xBE };
// The ComboSingleBag / mm-creation-new-file-world pinned seed.
static const char* const kPfcSeed = "RSBSSINGLEBAG1";
// OoT-only sentinel written into the live OoT half just before the Happy Mask
// Shop door; the return leg must find it (the half was RESTORED from the frozen
// state, not reloaded from the slot or regenerated). OoT's death counter is not
// a shared cross-game resource, and OoT's .sav on disk holds 0. Since #837 the
// .redsave does NOT: the crossing commit at the door writes the frozen half,
// sentinel included, so the sentinel no longer tells the frozen half apart
// from the .redsave's bytes; it still tells it apart from the .sav and from a
// regenerated half.
static const u16 kPfcDeathsSentinel = 777;
static uint32_t sPfcOoTWorldSeed = 0;
// The #800 S1 shop playtest drive (defined below, beside the #796 drive).
static bool PfcShopPlaytest(void);
static void PfcShopPlaytestSetCVars(bool on);
static uint16_t PfcShopPlaytestLogShelves(int scene, const char* when);
// The #577 M5 chest playtest drive (defined beside the shop drive).
static bool PfcChestPlaytest(void);
static void PfcChestPlaytestLogHosts(void);
// int-soaring-to-owl (#1169 XW6): the world's settings for the drive (defined
// with the drive, below).
static void SoarArmWorld(void);
// int-mm-warp-song-minuet (#1169 XW13): its world, and the checks on the file it
// created (defined with the row's OoT half, below).
static bool MwArmWorld(void);
static void MwGenerationFailed(void);
static bool MwCheckCreatedWorld(char* msg, size_t cap);

static bool PfcNoRefusalToast(char* msg, size_t cap) {
    const int toasts = OoT_Notification_EmittedCountForTest();
    for (int i = 0; i < toasts; i++) {
        char toast[256];
        if (OoT_Notification_EmittedAtForTest(i, toast, sizeof(toast)) &&
            strncmp(toast, RSBS_REFUSAL_TOAST_PREFIX, sizeof(RSBS_REFUSAL_TOAST_PREFIX) - 1) == 0) {
            snprintf(msg, cap, "a refusal toast was raised (toast %d of %d): \"%s\"", i + 1, toasts, toast);
            return false;
        }
    }
    return true;
}

/**
 * "Shipped defaults" means NOTHING the generation reads was chosen: every
 * option, trick and exclusion CVar of all three surfaces is unset, so each one
 * resolves to its compiled default. Rando_HeadlessSeedTest reads the live CVar
 * store (the build directory's persisted config), so a config that carries any
 * randomizer choice would generate a different world under the same log line.
 * Returns the number of explicit settings found and names the first few in
 * `msg`; 0 is the only passing answer.
 */
static int PfcExplicitWorldSettings(char* msg, size_t cap) {
    int found = 0;
    std::string names;
    auto note = [&](const std::string& cvar) {
        if (found < 4) {
            names += names.empty() ? cvar : ", " + cvar;
        }
        found++;
    };
    auto checkInt = [&](const std::string& cvar) {
        if (!cvar.empty() && Combo_CVarIsExplicitInt(cvar.c_str())) {
            note(cvar);
        }
    };
    // OoT: the option set Rando_HeadlessSeedTest builds (CreateOptions is what
    // it calls first; calling it here as well changes nothing it reads).
    auto settings = Rando::Settings::GetInstance();
    settings->CreateOptions();
    int ootOptions = 0;
    for (const auto& option : settings->GetAllOptions()) {
        ootOptions += option.GetCVarName().empty() ? 0 : 1;
        checkInt(option.GetCVarName());
    }
    for (int i = 0; i < RT_MAX; i++) {
        checkInt(settings->GetTrickOption(static_cast<RandomizerTrick>(i)).GetCVarName());
    }
    for (int i = 0; i < RC_MAX; i++) {
        Rando::Location* loc = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(i));
        if (loc != nullptr && loc->GetExcludedOption() != nullptr) {
            checkInt(loc->GetExcludedOption()->GetCVarName());
        }
    }
    for (const char* list : { CVAR_RANDOMIZER_SETTING("ExcludedLocations"), CVAR_RANDOMIZER_SETTING("EnabledTricks"),
                              CVAR_RANDOMIZER_SETTING("EnabledGlitches") }) {
        const char* value = CVarGetString(list, "");
        if (value != nullptr && value[0] != '\0') {
            note(list);
        }
    }
    // MM: the descriptor tables the options and tricks pages register (both
    // idempotent; re-registering the same table is silent).
    if (Combo_MMOptionCount() == 0) {
        MM_RandoOptionsUi_Register();
    }
    if (Combo_MMTrickCount() == 0) {
        MM_RandoTricksUi_Register();
    }
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* desc = Combo_MMOptionAt(i);
        if (desc != nullptr && desc->cvar != nullptr) {
            checkInt(desc->cvar);
        }
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* desc = Combo_MMTrickAt(i);
        if (desc != nullptr && desc->cvar != nullptr) {
            checkInt(desc->cvar);
        }
    }
    // The combo settings.
    for (int i = 0; i < (int)COMBO_SETTING_COUNT; i++) {
        int32_t value = 0;
        if (Combo_ComboSettingReadStore((ComboSettingId)i, &value)) {
            note(Combo_ComboSettingKey((ComboSettingId)i));
        }
    }
    if (ootOptions == 0 || Combo_MMOptionCount() == 0 || Combo_MMTrickCount() == 0) {
        // A surface with no descriptors would pass vacuously.
        snprintf(
            msg, cap,
            "the defaults check has nothing to check on some surface (OoT options %d, MM options %d, MM tricks %d)",
            ootOptions, Combo_MMOptionCount(), Combo_MMTrickCount());
        return -1;
    }
    snprintf(msg, cap,
             "%d explicit (%s%s); checked %d OoT options, %d OoT tricks, %d OoT exclusions, %d MM options, "
             "%d MM tricks, %d combo settings",
             found, names.empty() ? "none" : names.c_str(), found > 4 ? ", ..." : "", ootOptions, (int)RT_MAX,
             (int)RC_MAX, Combo_MMOptionCount(), Combo_MMTrickCount(), (int)COMBO_SETTING_COUNT);
    return found;
}

/**
 * The single-bag half of PfcCreationMatchesStore: finds the last kept
 * single-bag line the creation printed and parses its three crossing counts.
 * Returns 0 with the reason in `msg`. Exported for the ROM-free row
 * PfcCreationBagLine (#1135), which feeds it captured stderr.
 */
extern "C" int OoT_Pfc_CreationBagLine(char* bagLine, size_t bagCap, int* total, int* intoTermina, int* intoHyrule,
                                       char* msg, size_t cap) {
    // Two paths print it, with the same counts in the same places (#1135): the
    // creation event generating the paired world itself, and, since #1116,
    // Generate's own completion (OoT_PairedGen_CompleteWorld), which a creation
    // on a world still waiting at its general pass runs instead.
    if (!IntegrationTest_StderrCaptureLast("[OoT] creation event: single bag", bagLine, bagCap) &&
        !IntegrationTest_StderrCaptureLast("[OoT] paired completion: single bag", bagLine, bagCap)) {
        snprintf(msg, cap, "the creation never logged its single-bag line");
        return 0;
    }
    const char* p = strstr(bagLine, "single bag");
    while (*p != '\0' && (*p < '0' || *p > '9')) {
        p++;
    }
    if (sscanf(p, "%d crossings stored (%d OoT items in Termina, %d MM items in Hyrule)", total, intoTermina,
               intoHyrule) != 3) {
        snprintf(msg, cap, "the creation's single-bag line does not parse: \"%s\"", bagLine);
        return 0;
    }
    return 1;
}

/**
 * The creation's own account of the crossings, read off the lines it printed,
 * against the live store right after OoT_Sram_InitSave returned (before any
 * load): "[OoT] creation event: single bag — N crossings stored (X OoT items in
 * Termina, Y MM items in Hyrule)", or since #1116 the same line as "[OoT] paired
 * completion: single bag — ..." when Generate's completion finished the world
 * for the creation (#1135), and "[Crossings] captured A OoT-hosted and B
 * MM-hosted crossings (digest D)". Returns false with the reason in `msg`.
 */
static bool PfcCreationMatchesStore(char* msg, size_t cap) {
    char bagLine[512];
    char capLine[512];
    int total = -1;
    int intoTermina = -1;
    int intoHyrule = -1;
    if (!OoT_Pfc_CreationBagLine(bagLine, sizeof(bagLine), &total, &intoTermina, &intoHyrule, msg, cap)) {
        return false;
    }
    int capOoT = -1;
    int capMM = -1;
    unsigned capDigest = 0;
    const char* c = nullptr;
    if (IntegrationTest_StderrCaptureLast("[Crossings] captured", capLine, sizeof(capLine))) {
        c = strstr(capLine, "captured ");
    }
    if (c == nullptr ||
        sscanf(c, "captured %d OoT-hosted and %d MM-hosted crossings (digest %X)", &capOoT, &capMM, &capDigest) != 3) {
        snprintf(msg, cap, "the crossing store's capture line is missing or does not parse: \"%s\"",
                 c != nullptr ? capLine : "(none)");
        return false;
    }
    const int storeHyrule = Combo_Crossings_Count(GAME_OOT);
    const int storeTermina = Combo_Crossings_Count(GAME_MM);
    const unsigned storeDigest = (unsigned)Combo_Crossings_Digest();
    if (total <= 0 || total != intoTermina + intoHyrule || intoTermina != storeTermina || intoHyrule != storeHyrule ||
        capOoT != storeHyrule || capMM != storeTermina || capDigest != storeDigest) {
        snprintf(msg, cap,
                 "the creation's crossings disagree with the store it left: bag line %d = %d into Termina + %d into "
                 "Hyrule; capture line %d OoT-hosted, %d MM-hosted, digest %08X; store inHyrule=%d inTermina=%d "
                 "digest %08X",
                 total, intoTermina, intoHyrule, capOoT, capMM, capDigest, storeHyrule, storeTermina, storeDigest);
        return false;
    }
    snprintf(msg, cap, "%d crossings (%d OoT items in Termina, %d MM items in Hyrule), digest %08X", total, intoTermina,
             intoHyrule, storeDigest);
    return true;
}

static bool sPfcCreationAttempted = false;
// oot_file_select_identity_shot.cpp (#952, RSBS_PFC_FILE_SELECT_SHOT=1): after
// the creation, the file select opens the created file and draws its identity.
extern "C" int OoT_PfcFileSelectShot_Begin(FileChooseContext* fileChoose, int slot);
extern "C" int OoT_PfcFileSelectShot_Running(void);
extern "C" void OoT_PfcFileSelectShot_Frame(FileChooseContext* fileChoose);

// #1055: the pair's spoilers in the shared build directory (ForeignItemsSingleExe.cpp).
extern "C" int OoT_Creation_TestSpoilerSeedName(char* out, size_t cap);
extern "C" int OoT_Creation_TestClearPairedSpoiler(void);

/**
 * RSBS_PFC_PLANT_FOREIGN_SPOILER=1 (#1055; CTest IntPairedFirstCrossingForeignSpoiler):
 * after the drive clears this pair's spoilers, a spoiler of the SAME seed and
 * another identity is planted at Randomizer/<seed>.json: the identity this
 * generation froze with comboSettingsHash flipped, which is what a run of a
 * build with another fingerprint leaves behind. The creation must leave it as
 * it is and write its own spoiler to Randomizer/<seed>-<fingerprint>.json.
 */
static bool PfcPlantForeignSpoiler(void) {
    const char* env = getenv("RSBS_PFC_PLANT_FOREIGN_SPOILER");
    return env != NULL && strcmp(env, "1") == 0;
}
static std::string sPfcPlantedSeedName;
static std::string sPfcPlantedPath;
static std::string sPfcPlantedBytes;
static uint32_t sPfcPlantedFingerprint = 0;

static std::string PfcReadWholeFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == NULL) {
        return std::string();
    }
    std::string bytes;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        bytes.append(buf, n);
    }
    fclose(f);
    return bytes;
}

static bool PfcPlantForeignSpoilerWrite(char* msg, size_t cap) {
    char seed[64];
    if (!OoT_Creation_TestSpoilerSeedName(seed, sizeof(seed)) || seed[0] == '\0') {
        snprintf(msg, cap, "the generation has no seed name to plant a foreign spoiler under");
        return false;
    }
    sPfcPlantedSeedName = seed;
    sPfcPlantedFingerprint = ~gComboCtx.comboSettingsHash;
    if (sPfcPlantedFingerprint == 0) {
        sPfcPlantedFingerprint = 0x10550000u;
    }
    char doc[768];
    snprintf(doc, sizeof(doc),
             "{\n    \"_note\": \"planted by IntPairedFirstCrossingForeignSpoiler (#1055): this seed under another "
             "identity\",\n    \"rsbsSingleBagWorld\": true,\n    \"combo\": {\n        \"identity\": {\n            "
             "\"masterSeed\": %u,\n            \"ootSettingsHash\": %u,\n            \"mmProfileDigest\": %u,\n      "
             "      \"comboSettingsHash\": %u\n        }\n    }\n}\n",
             (unsigned)gComboCtx.sharedRandoSeed, (unsigned)gComboCtx.sharedRandoSettingsHash,
             (unsigned)gComboCtx.mmProfileDigest, (unsigned)sPfcPlantedFingerprint);
    std::error_code ec;
    std::filesystem::create_directories(Ship::Context::GetPathRelativeToAppDirectory("Randomizer"), ec);
    sPfcPlantedPath = Ship::Context::GetPathRelativeToAppDirectory("Randomizer/" + sPfcPlantedSeedName + ".json");
    FILE* f = fopen(sPfcPlantedPath.c_str(), "wb");
    if (f == NULL) {
        snprintf(msg, cap, "could not plant the foreign spoiler at '%s'", sPfcPlantedPath.c_str());
        return false;
    }
    fwrite(doc, 1, strlen(doc), f);
    fclose(f);
    sPfcPlantedBytes = PfcReadWholeFile(sPfcPlantedPath);
    fprintf(stderr, "[PFC] planted a spoiler of this seed under another identity (comboSettingsHash %08X) at '%s'\n",
            (unsigned)sPfcPlantedFingerprint, sPfcPlantedPath.c_str());
    fflush(stderr);
    return !sPfcPlantedBytes.empty();
}

/** After the creation: the planted spoiler is unchanged, and this world's went
 *  to Randomizer/<seed>-<fingerprint>.json, which gGeneral.SpoilerLog names and
 *  whose combo.identity is this world's. */
static bool PfcForeignSpoilerCheck(char* msg, size_t cap) {
    if (sPfcPlantedFingerprint == gComboCtx.comboSettingsHash) {
        snprintf(msg, cap, "the planted spoiler's fingerprint is the created world's (%08X), so the row proves nothing",
                 (unsigned)gComboCtx.comboSettingsHash);
        return false;
    }
    if (PfcReadWholeFile(sPfcPlantedPath) != sPfcPlantedBytes) {
        snprintf(msg, cap, "the creation rewrote the planted spoiler of another identity at '%s' (#564 V23)",
                 sPfcPlantedPath.c_str());
        return false;
    }
    char expected[160];
    snprintf(expected, sizeof(expected), "./Randomizer/%s-%08X.json", sPfcPlantedSeedName.c_str(),
             (unsigned)gComboCtx.comboSettingsHash);
    const std::string spoilerLog = CVarGetString(CVAR_GENERAL("SpoilerLog"), "");
    if (spoilerLog != expected) {
        snprintf(msg, cap,
                 "with another identity's spoiler at the seed name, gGeneral.SpoilerLog reads '%s' after the creation, "
                 "not '%s': this world's spoiler was not written beside it (#1055)",
                 spoilerLog.c_str(), expected);
        return false;
    }
    const std::string written =
        PfcReadWholeFile(Ship::Context::GetPathRelativeToAppDirectory(std::string(expected).substr(2)));
    char identityTerm[64];
    snprintf(identityTerm, sizeof(identityTerm), "\"comboSettingsHash\": %u", (unsigned)gComboCtx.comboSettingsHash);
    if (written.find("\"combo\"") == std::string::npos || written.find(identityTerm) == std::string::npos) {
        snprintf(msg, cap, "'%s' does not carry this world's joined combo section (%zu bytes)", expected,
                 written.size());
        return false;
    }
    snprintf(msg, cap, "the planted spoiler is unchanged and this world's is '%s' (%zu bytes, joined)", expected,
             written.size());
    return true;
}

// #1542 S4 (IntSongHostOoT / IntSongHostMM): decision A's two hosts, pinned so the
// rows drive them on every run instead of whichever song check the seed made a
// crossing. The debug-seed seam (Combo_Logic_TestSetForcedPlacements, the
// DebugSeedForcedPlacements row's) puts MM's Sonata of Awakening on OoT's Song of
// Time check (a transition trigger) for `oot`, and OoT's Zelda's Lullaby on MM's
// starting Song of Healing (armed at creation) for `mm`. Both are song rows of the
// shared song pool, so the pin is legal only while the pool is armed: the fill
// refuses it otherwise and the creation fails loudly. Set before the generation,
// kept through the creation's fill, cleared after it. 1 pinned, 0 not a song-host
// row, -1 refused.
extern "C" void MM_IntSongHost_PinIds(uint16_t* healingCheck, uint16_t* sonataItem); // MM GameExports

static int PfcSongHostPin(void) {
    const char* env = getenv("RSBS_PFC_SONG_HOST");
    if (env == NULL) {
        return 0;
    }
    ComboLogicForcedPlacement row;
    memset(&row, 0, sizeof(row));
    uint16_t healing = 0;
    uint16_t sonata = 0;
    MM_IntSongHost_PinIds(&healing, &sonata);
    if (strcmp(env, "oot") == 0) {
        row.hostGame = (uint8_t)GAME_OOT;
        row.hostCheck = (uint16_t)RC_SONG_FROM_OCARINA_OF_TIME;
        row.item.originGame = (uint8_t)GAME_MM;
        row.item.id = sonata;
    } else if (strcmp(env, "mm") == 0) {
        row.hostGame = (uint8_t)GAME_MM;
        row.hostCheck = healing;
        row.item.originGame = (uint8_t)GAME_OOT;
        row.item.id = (uint16_t)RG_ZELDAS_LULLABY;
    } else {
        return 0;
    }
    const int pinned = Combo_Logic_TestSetForcedPlacements(&row, 1);
    fprintf(stderr, "[SONG-HOST] pinned %s item %u on %s song check %u for this world (%d row)\n",
            Game_ToString((GameId)row.item.originGame), (unsigned)row.item.id, Game_ToString((GameId)row.hostGame),
            (unsigned)row.hostCheck, pinned);
    fflush(stderr);
    return pinned == 1 ? 1 : -1;
}

/**
 * The paired variant's boot: the production path a player takes, minus the menu
 * clicks. It runs from the REAL file select (FileChoose_Main's OnFileChooseMain,
 * with its own FileChooseContext), which the title reaches the way SoH's "Boot
 * Sequence: File Select" setting takes it there (GpPairedTitleToFileSelect), so
 * Title_Destroy's Sram_InitSram (SaveManager::Init: the Save directory, the slot
 * metadata) has run before anything is created, exactly as in play.
 *
 *   1. GENERATE the pinned paired world on the shipped defaults (no config beyond
 *      the OpenGL backend): Rando_HeadlessSeedTest(RSBSSINGLEBAG1), the same
 *      call and seed as the mm-creation-new-file-world row, then the
 *      seed-generated flag the menu's RandoMain::GenerateRando sets.
 *   2. CREATE the file through OoT's own new-file seam, OoT_Sram_InitSave (what
 *      the naming screen's "END" calls, z_file_nameset_NES.c): session retire,
 *      the slot arm, THE CREATION EVENT (OoT_Creation_AuthorRandoFile: MM's half
 *      authored and armed, the single bag, the crossings stored, the spoiler),
 *      Randomizer_InitSaveFile, and the slot's first write (.sav + .redsave).
 *   3. LOAD that file the way the file select does (FileChoose_LoadGame):
 *      Sram_OpenSave (Save_LoadFile, whose OnLoadFile reads the .redsave back:
 *      the frozen rules, the crossing store, the armed MM half), the same field
 *      resets, then OnLoadGame.
 *   4. PLACE the loaded file at the round trip's boot entrance (Market, 0x01D1,
 *      child, noon: the same spawn the debug-save repro uses) and enter Play.
 *
 * So the session plays slot 3's OoT half AS LOADED BACK FROM DISK, and MM's first
 * arrival hydrates the MM half the load re-armed from the .redsave.
 */
static void GpCreatePairedFileAndEnterPlay(FileChooseContext* fileChoose, const char* from) {
    const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
    char msg[512];
    if (fileChoose == NULL) {
        IntegrationTest_GameplayFail("no file-select GameState available for the paired creation");
        return;
    }
    GameState* gameState = &fileChoose->state;
    // Runs at most once: the file select's main keeps calling the boot
    // injection while the phase is still BOOT, and a failure below leaves it
    // there until the exit request lands. A second pass would erase the slot
    // the first one wrote (a failure keeps it for forensics) and generate again.
    if (sPfcCreationAttempted) {
        if (OoT_PfcFileSelectShot_Running()) {
            OoT_PfcFileSelectShot_Frame(fileChoose);
            return;
        }
        static bool sReentryLogged = false;
        if (!sReentryLogged) {
            sReentryLogged = true;
            fprintf(stderr, "[PFC] the file select re-entered the boot injection after the creation attempt; "
                            "not running it again\n");
            fflush(stderr);
        }
        return;
    }
    sPfcCreationAttempted = true;

    // ---- 0. the slot: empty, or this row's own file from an earlier run -----
    if (Save_Exist(kPfcSlot)) {
        const SaveFileMetaInfo* meta = Save_GetSaveMetaInfo(kPfcSlot);
        if (meta == NULL || memcmp(meta->playerName, kPfcName, sizeof(kPfcName)) != 0) {
            snprintf(msg, sizeof(msg),
                     "file slot %d already holds a save this row did not create; refusing to erase it (run the row "
                     "in a build directory without that save)",
                     kPfcSlot + 1);
            IntegrationTest_GameplayFail(msg);
            return;
        }
        fprintf(stderr, "[PFC] erasing this row's own file from an earlier run (slot %d)\n", kPfcSlot + 1);
        Save_DeleteFile(kPfcSlot);
    }

    // ---- 1. generate ---------------------------------------------------------
    // "The shipped defaults" is checked, not assumed: generation reads the live
    // CVar store, which here is the build directory's persisted config.
    const int explicitSettings = PfcExplicitWorldSettings(msg, sizeof(msg));
    if (explicitSettings != 0) {
        char reason[640];
        snprintf(reason, sizeof(reason),
                 "the world would not be generated on the shipped defaults: %s (run the row with a config that "
                 "carries no randomizer, MM or combo setting)",
                 msg);
        IntegrationTest_GameplayFail(reason);
        return;
    }
    fprintf(stderr, "[PFC] shipped defaults verified: %s\n", msg);
    if (IntegrationTest_SoaringToOwl()) {
        SoarArmWorld();
    }
    if (IntegrationTest_MMWarpSongMinuet() && !MwArmWorld()) {
        return;
    }
    if (PfcShopPlaytest()) {
        PfcShopPlaytestSetCVars(true);
    }
    const int songHostPin = PfcSongHostPin();
    if (songHostPin < 0) {
        IntegrationTest_GameplayFail("int-song-host: the debug-seed seam refused the song-host pin");
        return;
    }
    fprintf(stderr, "[PFC] generating the pinned paired world %s on the shipped defaults at %s\n", kPfcSeed, from);
    fflush(stderr);
    if (Rando_HeadlessSeedTest(kPfcSeed) != 0) {
        if (IntegrationTest_MMWarpSongMinuet()) {
            MwGenerationFailed();
        } else {
            IntegrationTest_GameplayFail("the pinned paired world did not generate");
        }
        return;
    }
    OTRGlobals::Instance->gRandoContext->SetSeedGenerated(true);
    if (!Combo_ForeignPairingActive()) {
        IntegrationTest_GameplayFail("generation on the shipped defaults published no cross-game pairing identity");
        return;
    }
    // #1055: the build directory is shared by every run of every build, and a
    // spoiler of this seed under another identity must not meet this creation
    // unless the row plants one on purpose.
    OoT_Creation_TestClearPairedSpoiler();
    if (PfcPlantForeignSpoiler() && !PfcPlantForeignSpoilerWrite(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return;
    }

    // ---- 2. create, through the naming screen's seam -------------------------
    // What the naming screen leaves set when the player confirms a name on file
    // 3 with the Randomizer quest selected (z_file_nameset_NES.c): the button,
    // the quest, the typed name in the slot's metadata, the slot as fileNum.
    fileChoose->buttonIndex = kPfcSlot;
    fileChoose->questType[kPfcSlot] = QUEST_RANDOMIZER;
    fileChoose->n64ddFlag = 0;
    memcpy(Save_GetSaveMetaInfo(kPfcSlot)->playerName, kPfcName, sizeof(kPfcName));
    gSaveContext.fileNum = kPfcSlot;
    const u16 dayTime = gSaveContext.dayTime;
    fprintf(stderr, "[PFC] creating file %d through OoT_Sram_InitSave (the production creation event)\n", kPfcSlot + 1);
    fflush(stderr);
    OoT_Sram_InitSave(fileChoose);
    gSaveContext.dayTime = dayTime;
    char line[512];
    snprintf(msg, sizeof(msg), "[OoT] creation event: slot %d complete", kPfcSlot);
    if (!Save_Exist(kPfcSlot) || !RsbsSave_HasSave(kPfcSlot) ||
        !IntegrationTest_StderrCaptureLast(msg, line, sizeof(line))) {
        snprintf(msg, sizeof(msg),
                 "the creation did not write a complete paired file (slot %d: .sav %s, .redsave %s, creation-complete "
                 "line %s)",
                 kPfcSlot + 1, Save_Exist(kPfcSlot) ? "written" : "MISSING",
                 RsbsSave_HasSave(kPfcSlot) ? "written" : "MISSING",
                 IntegrationTest_StderrCaptureCount("creation event: slot") > 0 ? "absent" : "never logged");
        IntegrationTest_GameplayFail(msg);
        return;
    }
    fprintf(stderr, "[PFC] created: \"%s\"\n", line);
    if (songHostPin > 0) {
        (void)Combo_Logic_TestSetForcedPlacements(NULL, 0); // the creation's fill took it
    }
    if (PfcShopPlaytest()) {
        PfcShopPlaytestSetCVars(false);
        PfcShopPlaytestLogShelves(-1, "created");
    }
    if (PfcChestPlaytest()) {
        PfcChestPlaytestLogHosts();
    }

    // ---- the identity AS CREATED, before anything is loaded back ---------------
    // The baseline every later check compares against is the creation's, not
    // the load's: a .redsave round trip that lost or reordered a crossing, or
    // changed a digest, must fail here rather than become the baseline.
    if (!PfcCreationMatchesStore(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return;
    }
    fprintf(stderr, "[PFC] creation's crossings = the store it left: %s\n", msg);
    if (!Combo_ForeignPairingActive() || gComboCtx.sharedRandoSeed == 0 || gComboCtx.mmProfileDigest == 0 ||
        gComboCtx.comboSettingsHash == 0) {
        snprintf(msg, sizeof(msg),
                 "the creation left no complete paired identity (paired=%d masterSeed=%u mmProfileDigest=%08X "
                 "comboFingerprint=%08X)",
                 Combo_ForeignPairingActive() ? 1 : 0, (unsigned)gComboCtx.sharedRandoSeed,
                 (unsigned)gComboCtx.mmProfileDigest, (unsigned)gComboCtx.comboSettingsHash);
        IntegrationTest_GameplayFail(msg);
        return;
    }
    IntegrationTest_PairedIdentityRecord();
    if (IntegrationTest_MMWarpSongMinuet() && !MwCheckCreatedWorld(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return;
    }
    if (PfcPlantForeignSpoiler()) {
        if (!PfcForeignSpoilerCheck(msg, sizeof(msg))) {
            IntegrationTest_GameplayFail(msg);
            return;
        }
        fprintf(stderr, "[PFC] foreign spoiler PASS: %s\n", msg);
    }

    // ---- the .sav the creation wrote names its record's world (#564 V12) -------
    // Read back from disk through the reader the file select's gate uses: SoH's
    // own "finalSeed" is the record's sharedRandoSeed, and the digest the
    // creation's commit mirrored ("rsbsPairSettingsHash") is the record's
    // sharedRandoSettingsHash. A writer that dropped the digest would leave 0
    // here, which the load reads as "not recorded" and never compares.
    {
        uint32_t savGeneration = 0;
        RsbsOoTSavIdentity savIdentity = { 0, 0 };
        SaveManager::Instance->ReadSavStamps(kPfcSlot, &savGeneration, &savIdentity);
        if (savIdentity.randoSeed != gComboCtx.sharedRandoSeed ||
            savIdentity.settingsHash != gComboCtx.sharedRandoSettingsHash) {
            snprintf(msg, sizeof(msg),
                     "the created .sav does not name its record's world (#564 V12): .sav finalSeed %u "
                     "rsbsPairSettingsHash %08X, record sharedRandoSeed %u sharedRandoSettingsHash %08X",
                     (unsigned)savIdentity.randoSeed, (unsigned)savIdentity.settingsHash,
                     (unsigned)gComboCtx.sharedRandoSeed, (unsigned)gComboCtx.sharedRandoSettingsHash);
            IntegrationTest_GameplayFail(msg);
            return;
        }
        fprintf(stderr,
                "[PFC] the created .sav names its record's world: finalSeed %u = sharedRandoSeed, "
                "rsbsPairSettingsHash %08X = sharedRandoSettingsHash (generation %u)\n",
                (unsigned)savIdentity.randoSeed, (unsigned)savIdentity.settingsHash, (unsigned)savGeneration);
    }

    // ---- RSBS_PFC_FILE_SELECT_SHOT=1 (#952): the file select shows the file ---
    // Instead of loading it: the next frames open file 3 at the file select and
    // capture its icons with the fingerprint beside them; the shot ends the run.
    if (OoT_PfcFileSelectShot_Begin(fileChoose, kPfcSlot)) {
        return;
    }

    // ---- 2b. RSBS_PFC_DIVERGE=1 (#804): the player changes settings ----------
    // Back at the title screen after the creation, one MM option, one MM trick
    // and one Cross-Game Rule move to other values. The load below must put the
    // file's values back (#781) before MM's first boot and its arrival gate.
    if (IntegrationTest_PairedDiverge() && !IntegrationTest_PairedDivergeApply(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return;
    }

    // ---- 3. load it back, as the file select does -----------------------------
    // The file select's gate first (#836): the player's A press on this file
    // runs it, so a healthy file (and, under RSBS_PFC_DIVERGE, a file whose
    // changed settings it can restore) must be accepted here.
    {
        char words[96];
        if (OoT_FileSelect_ProbeSlotForOpen(kPfcSlot, words, sizeof(words)) == 0) {
            snprintf(msg, sizeof(msg), "the file select's gate refused the paired file this row just created: \"%s\"",
                     words);
            IntegrationTest_GameplayFail(msg);
            return;
        }
        fprintf(stderr, "[PFC] the file select's gate accepted file %d (#836)\n", kPfcSlot + 1);
    }
    gSaveContext.fileNum = kPfcSlot;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    OoT_Sram_OpenSave();
    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, OoT_Play_Init, PlayState);
    gSaveContext.respawn[0].entranceIndex = ENTR_LOAD_OPENING;
    gSaveContext.respawnFlag = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    gSaveContext.eventInf[0] = 0;
    gSaveContext.eventInf[1] = 0;
    gSaveContext.eventInf[2] = 0;
    gSaveContext.eventInf[3] = 0;
    gSaveContext.unk_13EE = 0x32;
    gSaveContext.nayrusLoveTimer = 0;
    gSaveContext.healthAccumulator = 0;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
    gSaveContext.forcedSeqId = NA_BGM_GENERAL_SFX;
    gSaveContext.skyboxTime = 0;
    gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.chamberCutsceneNum = 0;
    gSaveContext.nextDayTime = 0xFFFF;
    gSaveContext.retainWeatherMode = 0;
    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }
    gSaveContext.forceRisingButtonAlphas = 0;
    gSaveContext.unk_13E8 = 0;
    gSaveContext.unk_13EA = 0;
    gSaveContext.unk_13EC = 0;
    gSaveContext.magicCapacity = 0;
    gSaveContext.magicFillTarget = gSaveContext.magic;
    gSaveContext.magic = 0;
    gSaveContext.magicLevel = gSaveContext.magic;
    gSaveContext.naviTimer = 0;
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);

    // ---- 4. the spawn the round trip boots into -------------------------------
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.sceneSetupIndex = 0;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.nightFlag = 0;
    gSaveContext.dayTime = 0x8000;
    gSaveContext.skyboxTime = 0x8000;
    gSaveContext.entranceIndex = cfg->bootEntrance;

    // ---- the loaded file is the paired world the creation authored ------------
    if (!IS_RANDO || !Combo_ForeignPairingActive() || !Context_HasFrozenState(GAME_MM) || !Combo_Crossings_IsFrozen() ||
        Combo_Crossings_Count(GAME_OOT) + Combo_Crossings_Count(GAME_MM) == 0) {
        snprintf(msg, sizeof(msg),
                 "the loaded file is not the paired world the creation wrote (rando=%d paired=%d mmHalfArmed=%d "
                 "crossingStoreFrozen=%d inHyrule=%d inTermina=%d)",
                 IS_RANDO ? 1 : 0, Combo_ForeignPairingActive() ? 1 : 0, Context_HasFrozenState(GAME_MM) ? 1 : 0,
                 Combo_Crossings_IsFrozen() ? 1 : 0, Combo_Crossings_Count(GAME_OOT), Combo_Crossings_Count(GAME_MM));
        IntegrationTest_GameplayFail(msg);
        return;
    }
    if (!PfcNoRefusalToast(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return;
    }
    char diff[512];
    if (!IntegrationTest_PairedIdentityMatches(diff, sizeof(diff))) {
        snprintf(msg, sizeof(msg), "the file loaded back from disk is not the world the creation authored: %s", diff);
        IntegrationTest_GameplayFail(msg);
        return;
    }
    fprintf(stderr, "[PFC] loaded identity = the creation's (masterSeed, settingsHash, mmProfileDigest, "
                    "comboFingerprint, crossing counts and digest)\n");
    if (IntegrationTest_PairedDiverge()) {
        // The load restored the file's values into the keys, and said so.
        char toasts[512];
        if (!IntegrationTest_PairedDivergeKeysHoldFile("after the load", msg, sizeof(msg))) {
            IntegrationTest_GameplayFail(msg);
            return;
        }
        if (!IntegrationTest_PairedDivergeLoadToasts(toasts, sizeof(toasts))) {
            snprintf(msg, sizeof(msg), "the load did not say what it restored: %s", toasts);
            IntegrationTest_GameplayFail(msg);
            return;
        }
        fprintf(stderr, "[PFC] diverged load PASS: %s; %s\n", msg, toasts);
    }
    sPfcOoTWorldSeed = Randomizer_GetCurrentWorldSeed();
    IntegrationTest_PairedSetMMGenerationBaseline(MM_Rando_OnSaveInitDispatchCount());
    fprintf(stderr,
            "[PFC] session plays file %d (slot %d) as loaded back from disk: OoT world seed %u, entering Play at "
            "entrance 0x%04X; MM generation dispatches so far %u (the creation's)\n",
            kPfcSlot + 1, kPfcSlot, (unsigned)sPfcOoTWorldSeed, cfg->bootEntrance,
            (unsigned)MM_Rando_OnSaveInitDispatchCount());
    fflush(stderr);
    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_PRE);
}

// ---- PLAYTEST DRIVE (#796), off unless RSBS_PFC_TRACKER_CROSSING=1 ----------
// The paired world's first MM item hosted in an OoT chest is opened through the
// real scene-flag hook a chest fires (RandomizerOnSceneFlagSetHandler queues the
// check, and the RC-queue drain gives it: the "You found" toast and the tracker's
// collected write). The drive logs the MM-bound shared-item count before and
// after, so the give is observed rather than inferred. The Check Tracker's search
// box is then filled with that chest's name (frame 90) and later with the
// crossed item's name (frame 600), so unattended window captures show its row
// under each search.
//
// #878: the chest itself is opened on EVERY paired run, not only under the
// variable, so the first crossing always carries a pickup and the leg's toast
// check (IntegrationTest_PairedLegToastsCheck) has an item the arrival could
// announce a second time. The tracker searches stay opt-in.
extern "C" int OoT_CheckTracker_TestSearch(const char* text);
extern "C" int OoT_CheckTracker_TestItemName(uint16_t rc, char* out, int cap);
extern "C" int OoT_CheckTracker_TestSearchShows(uint16_t rc, const char* text);

static void GpPairedTrackerSearch(int host, const std::string& text) {
    OoT_CheckTracker_TestSearch(text.c_str());
    char name[128] = "";
    OoT_CheckTracker_TestItemName((uint16_t)host, name, (int)sizeof(name));
    fprintf(stderr, "[PT796] check tracker filtered to \"%s\": status %d, lists the host %d, row names (%s)\n",
            text.c_str(),
            (int)OTRGlobals::Instance->gRandoContext->GetItemLocation((RandomizerCheck)host)->GetCheckStatus(),
            OoT_CheckTracker_TestSearchShows((uint16_t)host, text.c_str()), name);
    fflush(stderr);
}

// #575 H3b: the hint text's loaded-file gate, in play (3drando/menu.cpp). An OoT
// gossip stone that hints a crossing host names the MM item the host yields, with
// the paired file loaded and no creation gate open: what a player reads.
extern "C" int Rando_TestCheckLoadedCrossingHostHints(int* outProbes, int* outStones);

static void GpPairedCheckStoneHints(const char* leg) {
    int probes = 0;
    int stones = 0;
    const int failures = Rando_TestCheckLoadedCrossingHostHints(&probes, &stones);
    fprintf(stderr,
            "[PFC] %s: OoT stone hints in play name %d crossing host(s) by their MM item (%d loaded hint "
            "slot(s)), %d failure(s)\n",
            leg, probes, stones, failures);
    fflush(stderr);
    if (failures != 0 || probes == 0) {
        char msg[512];
        // int-soaring-to-owl generates the same seed with two settings moved
        // (songSoaringOot on, Shuffle Songs Anywhere), so its world is not the
        // one these checks were proven on: say so, or the red reads as Soaring's.
        snprintf(msg, sizeof(msg),
                 "%s: an OoT stone hint for a crossing host does not name the MM item it yields in play (%d "
                 "failure(s), %d host(s) probed; see [H3B])%s",
                 leg, failures, probes,
                 IntegrationTest_SoaringToOwl()
                     ? " [PFC] soaring world: songSoaringOot on, Shuffle Songs Anywhere; a fixture failure of the "
                       "paired checks, not a Soaring leg"
                 : IntegrationTest_MMWarpSongMinuet()
                     ? " [PFC] warp-song world: songMinuetMm on, Shuffle Songs Anywhere; a fixture failure of the "
                       "paired checks, not the Minuet's crossing"
                     : "");
        IntegrationTest_GameplayFail(msg);
    }
}

static void GpPairedTrackerPlaytest(int frame) {
    static int sHost = -1;
    static std::string sItemName;
    static int sSharedBefore = -1;
    const char* env = getenv("RSBS_PFC_TRACKER_CROSSING");
    const bool trackerDrive = env != NULL && strcmp(env, "1") == 0;
    if (frame == 30) {
        for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
            ComboCrossing row;
            if (!Combo_Crossings_At(GAME_OOT, i, &row)) {
                continue;
            }
            const Rando::SpoilerCollectionCheck cc =
                Rando::StaticData::GetLocation((RandomizerCheck)row.hostCheck)->GetCollectionCheck();
            if (cc.type != SPOILER_CHK_CHEST) {
                continue;
            }
            sHost = row.hostCheck;
            const char* item = Combo_GetForeignItemName(row.item);
            sItemName = item != NULL ? item : "";
            sSharedBefore = Combo_CountSharedItems(GAME_MM, true);
            fprintf(stderr,
                    "[PT796] opening OoT check %u (%s), which hosts the MM item %s: scene %u treasure flag %u\n",
                    (unsigned)row.hostCheck,
                    Rando::StaticData::GetLocation((RandomizerCheck)row.hostCheck)->GetShortName().c_str(),
                    item != NULL ? item : "?", (unsigned)cc.scene, (unsigned)cc.flag);
            fflush(stderr);
            GameInteractor_ExecuteOnSceneFlagSet(cc.scene, FLAG_SCENE_TREASURE, cc.flag);
            break;
        }
        if (sHost < 0 && IntegrationTest_SoaringToOwl()) {
            fprintf(stderr, "[PFC] soaring world: no OoT crossing row is hosted at a chest, so this leg carries no "
                            "pickup (the paired checks' fixture, not a Soaring leg)\n");
            fflush(stderr);
        }
        if (sHost < 0 && IntegrationTest_MMWarpSongMinuet()) {
            fprintf(stderr, "[PFC] warp-song world: no OoT crossing row is hosted at a chest, so this leg carries no "
                            "pickup (the paired checks' fixture, not the Minuet's crossing)\n");
            fflush(stderr);
        }
    }
    if (!trackerDrive) {
        return;
    }
    if (frame == 90 && sHost >= 0) {
        fprintf(stderr, "[PT796] MM-bound shared-item records: %d before the chest, %d after\n", sSharedBefore,
                Combo_CountSharedItems(GAME_MM, true));
        GpPairedTrackerSearch(sHost, Rando::StaticData::GetLocation((RandomizerCheck)sHost)->GetShortName());
    }
    if (frame == 600 && sHost >= 0 && !sItemName.empty()) {
        GpPairedTrackerSearch(sHost, sItemName);
    }
}

// ---- PLAYTEST DRIVE (#800 S1), off unless RSBS_PFC_OOT_SHOP=1 ---------------
// The paired world is generated with OoT's shopsanity (four emptied shelves per
// shop) and MM's shop shuffle on, set in-process for the generation and the
// creation and cleared again. Loading the file then restores MM's shop shuffle
// into the live CVars (the MM profile's load-time restore, Foreign.cpp), and the
// config saved at exit carries it: rewrite the build directory's config after a
// run (the PFC row refuses any explicit world setting).
// After the creation it logs every MM item the single bag put on an OoT shop
// shelf. In the post-return warp (point RSBS_GP_WARP_ENTRANCE at that shelf's
// shop) it logs, per shelf of the scene, whether OoT draws the MM model or the
// stand-in, opens that shelf's description textbox at frame 100 (closed at 700),
// and at frame 760 buys the first such shelf the way
// EnGirlA_ItemGive_Randomizer does (the shelf's RandomizerInf flag, minus the
// charge), so the drain's pickup toast and the MM-bound record are observed.
// The flag write bypasses the shopkeeper, so at frame 920 the drive does to the
// bought shelf what z_en_ossan.c does after a real purchase: setOutOfStockFunc
// when the purchase goes through (OoT_EnOssan_HandleCanBuyItem), then updateStockedItemFunc
// when shopping continues (EnOssan_State_ContinueShoppingPrompt), whose
// EnGirlA_TryChangeShopItemShip decides SOLD OUT. Frame 990 logs and captures it.
extern "C" int OoT_ForeignModel_ModelForOoTCheck(uint16_t rc, ComboModel* out);
extern "C" int OoT_Rando_Foreign_HostCollected(uint16_t rc);
extern "C" void Flags_SetRandomizerInf(RandomizerInf flag); // z_actor.c
extern "C" void OoT_Message_StartTextbox(PlayState* play, u16 textId, Actor* actor);
extern "C" void OoT_Message_CloseTextbox(PlayState* play);

static const char* const kPfcShopCVars[][2] = {
    { CVAR_RANDOMIZER_SETTING("Shopsanity"), "1" },      // RO_SHOPSANITY_SPECIFIC_COUNT
    { CVAR_RANDOMIZER_SETTING("ShopsanityCount"), "4" }, // four emptied shelves per shop
    { "gRando.Options.RO_SHUFFLE_SHOPS", "1" },          // MM's shop stock joins the bag
};

static bool PfcShopPlaytest(void) {
    const char* env = getenv("RSBS_PFC_OOT_SHOP");
    return env != NULL && strcmp(env, "1") == 0;
}

static void PfcShopPlaytestSetCVars(bool on) {
    for (const auto& cv : kPfcShopCVars) {
        if (on) {
            CVarSetInteger(cv[0], atoi(cv[1]));
        } else {
            CVarClear(cv[0]);
        }
    }
    fprintf(stderr, "[S1-PLAYTEST] shop shuffles %s in-process (OoT shopsanity 4 per shop, MM shop shuffle)\n",
            on ? "SET" : "cleared");
    fflush(stderr);
}

/** Every MM item on an OoT shop shelf; `scene` < 0 for all scenes. Returns the
 *  first such check in the scene (0 for none). */
static uint16_t PfcShopPlaytestLogShelves(int scene, const char* when) {
    uint16_t first = 0;
    int shown = 0;
    for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
        ComboCrossing row;
        if (!Combo_Crossings_At(GAME_OOT, i, &row)) {
            continue;
        }
        Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)row.hostCheck);
        if (loc == nullptr || loc->GetRCType() != RCTYPE_SHOP || (scene >= 0 && loc->GetScene() != scene)) {
            continue;
        }
        ComboModel model;
        const int drawable = OoT_ForeignModel_ModelForOoTCheck(row.hostCheck, &model);
        ComboModelAnswer answer;
        const uint8_t kind = Combo_GetForeignItemModel((uint8_t)GAME_OOT, row.item, &answer);
        const char* item = Combo_GetForeignItemName(row.item);
        fprintf(
            stderr,
            "[S1-PLAYTEST] %s: MM %s on OoT %s (check %u, scene %d, price %u): model answer %s, shelf draws %s%s\n",
            when, item != NULL ? item : "?", loc->GetName().c_str(), (unsigned)row.hostCheck, (int)loc->GetScene(),
            (unsigned)OTRGlobals::Instance->gRandoContext->GetItemLocation((RandomizerCheck)row.hostCheck)->GetPrice(),
            kind == COMBO_MODEL_ANSWER_DESCRIPTOR    ? "DESCRIPTOR"
            : kind == COMBO_MODEL_ANSWER_HOST_NATIVE ? "HOST_NATIVE"
                                                     : "NONE",
            drawable ? "a model, first list " : "the stand-in (mystery item)", drawable ? model.parts[0].dl : "");
        first = first == 0 ? row.hostCheck : first;
        shown++;
    }
    fprintf(stderr, "[S1-PLAYTEST] %s: %d MM item(s) on OoT shop shelves%s\n", when, shown,
            scene >= 0 ? " in this scene" : "");
    fflush(stderr);
    return first;
}

// ---- PLAYTEST DRIVE (#577 M5), off unless RSBS_PFC_OOT_CHEST=1 --------------
// After the creation it logs every OoT chest that hosts an MM item (its scene),
// so a run can point RSBS_GP_WARP_ENTRANCE at one. In the post-return warp it
// turns "Chest Size & Texture Matches Contents" on in-process (the staged config
// is rewritten after a run) and at frame 40 logs every chest actor of the scene:
// its check, whether it hosts an MM item, and the category OoT_EnBox_Init left
// on its entry, the one the texture swap reads.
// OTRGlobals.cpp; OTRGlobals.h declares it for C translation units only.
extern "C" RandomizerCheck Randomizer_GetCheckFromActor(s16 actorId, s16 sceneNum, s16 actorParams);
extern "C" ShopItemIdentity Randomizer_IdentifyShopItem(s32 sceneNum, u8 slotIndex);

static bool PfcChestPlaytest(void) {
    const char* env = getenv("RSBS_PFC_OOT_CHEST");
    return env != NULL && strcmp(env, "1") == 0;
}

static void PfcChestPlaytestLogHosts(void) {
    int shown = 0;
    int notProgression = 0;
    for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
        ComboCrossing row;
        if (!Combo_Crossings_At(GAME_OOT, i, &row)) {
            continue;
        }
        // The one game-neutral grade OoT can read for an MM item (shared_items.h);
        // the chest's major presentation rests on every crossing being PROGRESSION.
        if (Combo_ItemClassOf(row.item) != RSBS_FILL_CLASS_PROGRESSION) {
            const char* odd = Combo_GetForeignItemName(row.item);
            fprintf(stderr, "[M5-PLAYTEST] created: crossing MM %s on OoT check %u has fill class %s\n",
                    odd != NULL ? odd : "?", (unsigned)row.hostCheck, Combo_ItemClassName(Combo_ItemClassOf(row.item)));
            notProgression++;
        }
        Rando::Location* loc = Rando::StaticData::GetLocation((RandomizerCheck)row.hostCheck);
        if (loc == nullptr || loc->GetCollectionCheck().type != SPOILER_CHK_CHEST) {
            continue;
        }
        const char* item = Combo_GetForeignItemName(row.item);
        fprintf(stderr, "[M5-PLAYTEST] created: MM %s in OoT chest %s (check %u, scene %d)\n",
                item != NULL ? item : "?", loc->GetName().c_str(), (unsigned)row.hostCheck, (int)loc->GetScene());
        shown++;
    }
    fprintf(stderr,
            "[M5-PLAYTEST] created: %d MM item(s) in OoT chests; %d of %d OoT-hosted crossing(s) not PROGRESSION\n",
            shown, notProgression, Combo_Crossings_Count(GAME_OOT));
    fflush(stderr);
}

static void PfcChestPlaytestWarpFrame(PlayState* play, int frame) {
    if (!PfcChestPlaytest()) {
        return;
    }
    if (frame == 1) {
        CVarSetInteger(CVAR_ENHANCEMENT("ChestSizeAndTextureMatchContents"), 1);
    }
    if (frame != 40) {
        return;
    }
    int chests = 0;
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_CHEST].head; a != NULL; a = a->next) {
        if (a->id != ACTOR_EN_BOX) {
            continue;
        }
        const RandomizerCheck rc = Randomizer_GetCheckFromActor(a->id, play->sceneNum, a->params);
        const SharedItem* hosted = rc != RC_UNKNOWN_CHECK ? Combo_GetForeignPlacementForOoTCheck((uint16_t)rc) : NULL;
        const char* item = hosted != NULL ? Combo_GetForeignItemName(*hosted) : NULL;
        fprintf(stderr,
                "[M5-PLAYTEST] chest actor at (%.0f, %.0f, %.0f): check %d (%s), hosts %s%s; entry category %d (%s)\n",
                a->world.pos.x, a->world.pos.y, a->world.pos.z, (int)rc,
                rc != RC_UNKNOWN_CHECK ? Rando::StaticData::GetLocation(rc)->GetName().c_str() : "?",
                hosted != NULL ? "the MM item " : "no MM item", item != NULL ? item : "",
                (int)((EnBox*)a)->getItemEntry.getItemCategory,
                ((EnBox*)a)->getItemEntry.getItemCategory == ITEM_CATEGORY_MAJOR  ? "major"
                : ((EnBox*)a)->getItemEntry.getItemCategory == ITEM_CATEGORY_JUNK ? "junk"
                                                                                  : "other");
        chests++;
    }
    fprintf(stderr, "[M5-PLAYTEST] warp arrival: %d chest actor(s) in scene %d, CSMC on\n", chests,
            (int)play->sceneNum);
    fflush(stderr);
}

// ---- #1542 S3: an MM item on an OoT SONG CHECK, delivered live -----------------
// RSBS_PFC_SONG_HOST=oot (CTest IntSongHostOoT). Under the shared song pool an OoT
// song check can hold an MM song. At first-leg frame 200 (the leg's chest pickup
// has played out) the drive takes the Song of Time check, a transition trigger
// (RSBS_SONG_GIVE_OOT_TRIGGER_FLAG), which PfcSongHostPin made hold MM's Sonata of
// Awakening for this world, and sets that check's collection flag
// the way its scene does: its EventChkInf flag (ShuffleSongs.cpp) through
// OoT_Flags_SetEventChkInf, or a RandomizerInf flag through Flags_SetRandomizerInf,
// whose flag-set hook queues the check for the RC-queue drain; any other collection
// type is queued as the hooks queue it. Frame 320: the drain's foreign branch must have
// recorded exactly one MM-bound item and the host must read collected; the check
// is then queued again (a second trigger). Frame 440: no second record. A textbox
// nobody will press past is closed after 60 frames. Not covered here: the trigger
// cutscene itself and each scene's progression after it (traced in S0's table; a
// playtest item).
extern "C" int OoT_ComboLogic_TestSongCheckIndex(uint16_t rc);
extern "C" int OoT_Rando_Foreign_PlaytestQueueCheck(uint16_t rc); // hook_handlers.cpp
extern "C" void OoT_Flags_SetEventChkInf(s32 flag);               // z_actor.c

static bool GpSongHostOoT(void) {
    const char* env = getenv("RSBS_PFC_SONG_HOST");
    return env != NULL && strcmp(env, "oot") == 0;
}

static void GpSongHostOoTFrame(PlayState* play, int frame) {
    static int sHost = -1;
    static int sRecordsBefore = -1;
    static int sRecordsMid = -1;
    static int sTextboxFrames = 0;
    if (play != NULL && play->msgCtx.msgMode != MSGMODE_NONE) {
        if (++sTextboxFrames >= 60) {
            OoT_Message_CloseTextbox(play);
            sTextboxFrames = 0;
        }
    } else {
        sTextboxFrames = 0;
    }
    if (frame == 200) {
        // Decision A's OoT host, pinned by PfcSongHostPin: the Song of Time check
        // (a transition trigger) holds MM's Sonata of Awakening on every run.
        const uint16_t pinnedHost = (uint16_t)RC_SONG_FROM_OCARINA_OF_TIME;
        if (Combo_Crossings_Lookup(GAME_OOT, pinnedHost) == NULL) {
            IntegrationTest_GameplayFail("int-song-host-oot: the pinned Song of Time check holds no MM item (the "
                                         "shared song pool is not armed, or the pin was dropped: a fixture failure)");
            return;
        }
        if (OoT_Rando_Foreign_HostCollected(pinnedHost)) {
            IntegrationTest_GameplayFail("int-song-host-oot: the pinned Song of Time check was collected before the "
                                         "drive set its flag (the leg's route moved: a fixture failure)");
            return;
        }
        sHost = (int)pinnedHost;
        const SharedItem* item = Combo_Crossings_Lookup(GAME_OOT, (uint16_t)sHost);
        const char* name = item != NULL ? Combo_GetForeignItemName(*item) : NULL;
        const Rando::SpoilerCollectionCheck cc =
            Rando::StaticData::GetLocation((RandomizerCheck)sHost)->GetCollectionCheck();
        sRecordsBefore = Combo_CountSharedItems(GAME_MM, true);
        fprintf(stderr,
                "[SONG-HOST] OoT song check %d (%s) hosts the MM item %s: collection type %d flag %d; MM-bound "
                "records %d\n",
                sHost, Rando::StaticData::GetLocation((RandomizerCheck)sHost)->GetShortName().c_str(),
                name != NULL ? name : "?", (int)cc.type, (int)cc.flag, sRecordsBefore);
        fflush(stderr);
        if (cc.type == SPOILER_CHK_EVENT_CHK_INF) {
            OoT_Flags_SetEventChkInf((s32)cc.flag); // the song checks' own type (ShuffleSongs.cpp)
        } else if (cc.type == SPOILER_CHK_RANDOMIZER_INF) {
            Flags_SetRandomizerInf((RandomizerInf)cc.flag);
        } else {
            (void)OoT_Rando_Foreign_PlaytestQueueCheck((uint16_t)sHost);
        }
    }
    if (sHost < 0) {
        return;
    }
    if (frame == 320) {
        const int records = Combo_CountSharedItems(GAME_MM, true);
        const int collected = OoT_Rando_Foreign_HostCollected((uint16_t)sHost);
        fprintf(stderr, "[SONG-HOST] frame 320: OoT song check %d collected %d, MM-bound records %d -> %d\n", sHost,
                collected, sRecordsBefore, records);
        fflush(stderr);
        if (collected == 0 || records != sRecordsBefore + 1) {
            IntegrationTest_GameplayFail("int-song-host-oot: the RC-queue drain did not deliver the MM item on the "
                                         "OoT song check exactly once (see [SONG-HOST])");
            return;
        }
        sRecordsMid = records;
        (void)OoT_Rando_Foreign_PlaytestQueueCheck((uint16_t)sHost); // a second trigger of the same check
    }
    if (frame == 440) {
        const int records = Combo_CountSharedItems(GAME_MM, true);
        fprintf(stderr, "[SONG-HOST] frame 440: after a second trigger MM-bound records %d (were %d)\n", records,
                sRecordsMid);
        fflush(stderr);
        if (records != sRecordsMid) {
            IntegrationTest_GameplayFail("int-song-host-oot: a second trigger of a delivered OoT song check recorded "
                                         "its MM item again");
        }
    }
}

// The game framebuffer as a PNG in RSBS_GP_SHOT_DIR (a no-op without it). The
// function lives in MM's export TU but reads only the shared window's Fast3D
// interpreter, so it captures whichever game is drawing: OoT here. An outside
// window capture is black while the desktop is locked (#843).
extern "C" void MM_Playtest_DumpGameFramebuffer(const char* tag);

// The shelf actor of the scene that sells OoT check `rc` (NULL when none).
static EnGirlA* PfcShopPlaytestFindShelf(PlayState* play, uint16_t rc) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head; a != NULL; a = a->next) {
        if (a->id != ACTOR_EN_GIRLA) {
            continue;
        }
        EnGirlA* shelf = (EnGirlA*)a;
        if (Randomizer_IdentifyShopItem(play->sceneNum, shelf->randoSlotIndex).identity.randomizerCheck ==
            (RandomizerCheck)rc) {
            return shelf;
        }
    }
    return NULL;
}

static void PfcShopPlaytestLogShelfState(PlayState* play, uint16_t rc, const char* when) {
    EnGirlA* shelf = PfcShopPlaytestFindShelf(play, rc);
    if (shelf == NULL) {
        fprintf(stderr, "[S1-PLAYTEST] %s: no shelf actor sells check %u\n", when, (unsigned)rc);
    } else {
        fprintf(stderr, "[S1-PLAYTEST] %s: shelf of check %u: params %d (%s), drawn %d, text 0x%04X\n", when,
                (unsigned)rc, (int)shelf->actor.params, shelf->actor.params == SI_SOLD_OUT ? "SOLD OUT" : "stocked",
                shelf->actor.draw != NULL ? 1 : 0, (unsigned)shelf->actor.textId);
    }
    fflush(stderr);
}

static void PfcShopPlaytestWarpFrame(PlayState* play, int frame) {
    static uint16_t sShelf = 0;
    static int sSharedBefore = -1;
    PfcChestPlaytestWarpFrame(play, frame);
    if ((PfcShopPlaytest() || PfcChestPlaytest()) && (frame == 60 || frame == 400 || frame == 900 || frame == 990)) {
        char tag[48];
        snprintf(tag, sizeof(tag), "oot-warp-scene%d-frame%d", (int)play->sceneNum, frame);
        MM_Playtest_DumpGameFramebuffer(tag);
    }
    if (!PfcShopPlaytest()) {
        return;
    }
    if (frame == 40) {
        sShelf = PfcShopPlaytestLogShelves(play->sceneNum, "warp arrival");
    }
    if (frame == 100 && sShelf != 0) {
        // The shelf's own description textbox: the id z_en_girla.c gives a
        // randomized shelf (TEXT_SHOP_ITEM_RANDOM + its shop-item index), which
        // BuildShopMessage (MerchantMessages.cpp) builds.
        const Rando::SpoilerCollectionCheck cc =
            Rando::StaticData::GetLocation((RandomizerCheck)sShelf)->GetCollectionCheck();
        const u16 textId = (u16)(0x9100 + ((int)cc.flag - (int)RAND_INF_SHOP_ITEMS_KF_SHOP_ITEM_1));
        fprintf(stderr, "[S1-PLAYTEST] opening the shelf's description textbox 0x%04X (check %u)\n", (unsigned)textId,
                (unsigned)sShelf);
        fflush(stderr);
        OoT_Message_StartTextbox(play, textId, NULL);
    }
    if (frame == 700 && sShelf != 0) {
        OoT_Message_CloseTextbox(play);
    }
    if (frame == 760 && sShelf != 0) {
        const Rando::SpoilerCollectionCheck cc =
            Rando::StaticData::GetLocation((RandomizerCheck)sShelf)->GetCollectionCheck();
        sSharedBefore = Combo_CountSharedItems(GAME_MM, true);
        fprintf(stderr,
                "[S1-PLAYTEST] buying OoT check %u: setting its RandomizerInf flag %u as EnGirlA_ItemGive_Randomizer "
                "does (sold out before: %d)\n",
                (unsigned)sShelf, (unsigned)cc.flag, OoT_Rando_Foreign_HostCollected(sShelf));
        fflush(stderr);
        Flags_SetRandomizerInf((RandomizerInf)cc.flag);
    }
    if (frame == 900 && sShelf != 0) {
        fprintf(stderr,
                "[S1-PLAYTEST] MM-bound shared-item records: %d before the purchase, %d after; sold out now: "
                "%d\n",
                sSharedBefore, Combo_CountSharedItems(GAME_MM, true), OoT_Rando_Foreign_HostCollected(sShelf));
        fflush(stderr);
        PfcShopPlaytestLogShelfState(play, sShelf, "after the flag write");
    }
    if (frame == 920 && sShelf != 0) {
        EnGirlA* shelf = PfcShopPlaytestFindShelf(play, sShelf);
        if (shelf != NULL) {
            fprintf(stderr, "[S1-PLAYTEST] restocking check %u's shelf as z_en_ossan.c does after a purchase\n",
                    (unsigned)sShelf);
            fflush(stderr);
            shelf->setOutOfStockFunc(play, shelf);
            shelf->updateStockedItemFunc(play, shelf);
        }
        PfcShopPlaytestLogShelfState(play, sShelf, "after the restock");
    }
    if (frame == 990 && sShelf != 0) {
        PfcShopPlaytestLogShelfState(play, sShelf, "frame 990");
    }
}

// ---- PLAYTEST DRIVE (#577 M4), off unless RSBS_PFC_OOT_FOREIGN_MODEL=1 -------
// In the post-return warp (the OoT play window after the round trip, whose frames
// the unattended captures show), the first OoT check hosting an MM item whose
// model OoT can draw right now (OoT_ForeignModel_ModelForOoTCheck: MM's
// DESCRIPTOR, or since #832 OoT's own row for a HOST_NATIVE answer, every part
// of which a mounted archive holds) is queued into the RC queue at frame 100,
// exactly as the scene-flag and RandomizerInf hooks queue a check the player
// opens. The drain records the crossing and OoT's get-item cutscene shows the MM
// model; nothing is at the keyboard, so the textbox is closed at frame 500 (or 40
// frames before the phase ends). Logged: the MM-bound shared-item records before
// and after (written by the drain) and whether the check is collected; the give
// point's own log line ("... gives nothing (#577 M4)") is what shows its take.
extern "C" int OoT_Rando_Foreign_PlaytestQueueCheck(uint16_t rc); // hook_handlers.cpp

static void PfcForeignModelPlaytestFrame(PlayState* play, int frame, int phaseFrames) {
    static uint16_t sHost = 0;
    static int sSharedBefore = -1;
    const char* env = getenv("RSBS_PFC_OOT_FOREIGN_MODEL");
    if (env == NULL || strcmp(env, "1") != 0) {
        return;
    }
    if (frame == 100) {
        for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
            ComboCrossing row;
            ComboModel model;
            if (!Combo_Crossings_At(GAME_OOT, i, &row) || OoT_Rando_Foreign_HostCollected(row.hostCheck) ||
                OoT_ForeignModel_ModelForOoTCheck(row.hostCheck, &model) != 1) {
                continue;
            }
            sHost = row.hostCheck;
            sSharedBefore = Combo_CountSharedItems(GAME_MM, true);
            const char* item = Combo_GetForeignItemName(row.item);
            fprintf(stderr,
                    "[M4-PLAYTEST] queuing OoT check %u (%s), which hosts the MM item %s: %u part(s), first %s "
                    "(crossing %d of %d)\n",
                    (unsigned)row.hostCheck,
                    Rando::StaticData::GetLocation((RandomizerCheck)row.hostCheck)->GetName().c_str(),
                    item != NULL ? item : "?", (unsigned)model.partCount, model.parts[0].dl, i + 1,
                    Combo_Crossings_Count(GAME_OOT));
            fflush(stderr);
            OoT_Rando_Foreign_PlaytestQueueCheck(row.hostCheck);
            break;
        }
        if (sHost == 0) {
            fprintf(stderr, "[M4-PLAYTEST] no OoT-hosted crossing of %d has a drawable MM model\n",
                    Combo_Crossings_Count(GAME_OOT));
            fflush(stderr);
        }
    }
    const int closeAt = phaseFrames - 40 < 500 ? phaseFrames - 40 : 500;
    if (frame == closeAt && sHost != 0) {
        fprintf(stderr, "[M4-PLAYTEST] closing the get-item textbox (frame %d)\n", frame);
        fflush(stderr);
        OoT_Message_CloseTextbox(play);
    }
    if (frame == closeAt + 20 && sHost != 0) {
        fprintf(stderr, "[M4-PLAYTEST] MM-bound shared-item records: %d before, %d after; check collected %d\n",
                sSharedBefore, Combo_CountSharedItems(GAME_MM, true), OoT_Rando_Foreign_HostCollected(sHost));
        fflush(stderr);
    }
}

static bool sPfcTitleRedirected = false;

/**
 * The paired variant's title: straight to the file select, as SoH's "Boot
 * Sequence: File Select" option does it (CustomLogoTitle.cpp,
 * OnZTitleUpdateSkipToFileSelect). Title_Destroy runs on the way out, and with
 * it Sram_InitSram.
 */
static void GpPairedTitleToFileSelect(GameState* gameState) {
    if (sPfcTitleRedirected || gameState == NULL) {
        return;
    }
    sPfcTitleRedirected = true;
    fprintf(stderr, "[PFC] title -> file select (SoH's Boot Sequence: File Select path)\n");
    fflush(stderr);
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, FileChoose_Init, FileChooseContext);
}

/**
 * The boot injection, by variant: the debug save (the round trip, and the
 * paired row's red half), or the paired creation, which needs the file select.
 * `isFileSelect` says which gamestate `gameState` is.
 */
static void GpBootInject(GameState* gameState, const char* from, bool isFileSelect) {
    if (IntegrationTest_PairedFirstCrossing() && !IntegrationTest_PairedSkipCreation()) {
        if (isFileSelect) {
            GpCreatePairedFileAndEnterPlay((FileChooseContext*)gameState, from);
        } else {
            GpPairedTitleToFileSelect(gameState);
        }
    } else {
        GpInjectDebugSaveAndEnterPlay(gameState, from);
    }
}

/**
 * The paired variant's return leg (OoT scene init, Market from the Mask Shop):
 * OoT's half came back RESTORED from the frozen state, not reloaded or
 * regenerated, under the same identity. Returns false after failing the run.
 */
static bool GpPairedCheckReturn(void) {
    char msg[768];
    char diff[512];
    if (!IS_RANDO || gSaveContext.fileNum != kPfcSlot) {
        snprintf(msg, sizeof(msg), "the return leg's OoT half is not the created file (rando=%d fileNum=%d)",
                 IS_RANDO ? 1 : 0, (int)gSaveContext.fileNum);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    const uint32_t worldSeed = Randomizer_GetCurrentWorldSeed();
    if (worldSeed != sPfcOoTWorldSeed) {
        snprintf(msg, sizeof(msg), "OoT's world changed across the crossing (seed %u -> %u): regenerated",
                 (unsigned)sPfcOoTWorldSeed, (unsigned)worldSeed);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (gSaveContext.deaths != kPfcDeathsSentinel) {
        snprintf(msg, sizeof(msg),
                 "OoT's half was not restored from the frozen state: the sentinel written before the Happy Mask Shop "
                 "door (deaths=%u) came back as %u",
                 (unsigned)kPfcDeathsSentinel, (unsigned)gSaveContext.deaths);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (!IntegrationTest_PairedIdentityMatches(diff, sizeof(diff))) {
        snprintf(msg, sizeof(msg), "the paired identity changed across the round trip: %s", diff);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (MM_Rando_OnSaveInitDispatchCount() != IntegrationTest_PairedMMGenerationBaseline()) {
        snprintf(msg, sizeof(msg), "MM's generation dispatch ran during the round trip (%u -> %u)",
                 (unsigned)IntegrationTest_PairedMMGenerationBaseline(), (unsigned)MM_Rando_OnSaveInitDispatchCount());
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    char line[512];
    if (IntegrationTest_StderrCaptureLast("REFUSED", line, sizeof(line))) {
        snprintf(msg, sizeof(msg), "a refusal was logged during the round trip: \"%s\"", line);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    if (!PfcNoRefusalToast(msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    // #878: the MM pickup before the Clock Tower door was announced there; the
    // arrival here must not toast it again, and no text repeats in the leg.
    if (!IntegrationTest_PairedLegToastsCheck("Ocarina of Time", msg, sizeof(msg))) {
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    fprintf(stderr, "[PFC] return leg toasts PASS: %s\n", msg);
    char desc[256];
    IntegrationTest_PairedIdentityDescribe(IntegrationTest_PairedIdentityRecorded(), desc, sizeof(desc));
    fprintf(stderr,
            "[PFC] return leg PASS: OoT's half restored, not regenerated (file %d, OoT world seed %u, sentinel "
            "deaths=%u survived), same identity: %s\n",
            kPfcSlot + 1, (unsigned)worldSeed, (unsigned)gSaveContext.deaths, desc);
    fflush(stderr);
    return true;
}

// ============================================================================
// int-soaring-to-owl (#1169 XW6): OoT's Song of Soaring, played for real
// ============================================================================
//
// The paired first crossing (every check above runs unchanged) with the hidden
// songSoaringOot rule revealed. The song is played the way a player plays it:
// the Fairy Ocarina on C-left, pressed, then the six notes, all through SoH's
// emulated buttons (GameInteractor_SetEmulatedButtons, which padmgr.c ORs into
// the controller's state, so both the message system's input and the ocarina's
// own read, Audio_GetOcaInput, see real presses). Nothing calls the matcher,
// the gates or the chooser directly: code_800EC960.c's matcher and the four
// z_message_PAL.c Soaring arms run in their real frame order, and the drive only
// watches the message context they leave.
//
// The first OoT leg (Market, then the Windmill), after the paired checks' own
// frames:
//   ADULT       the age the matcher reads is adult while the notes are played:
//               nothing matches, the ocarina stays in MSGMODE_OCARINA_PLAYING,
//               and only B closes it (OCARINA_MODE_04, nothing armed).
//   NO_STATUES  a child in the Market, MM's half the creation shadow (day 0, no
//               owl): "You played the Song of Soaring", then the no-statues text,
//               OCARINA_MODE_04, nothing armed, control back.
//   RESTRICTED  a child in the Windmill (warp-song restriction 3): "You played",
//               then 0x88C, OCARINA_MODE_04, nothing armed, control back.
// The MM leg (games/mm/2s2h/GameExports_SingleExe.cpp) activates the Clock Town
// and swamp owls before the Clock Tower door. The return leg (Market):
//   CHOOSER     XW6's text chooser, the owl map's fallback, kept live: with the
//               map's assets forced absent (OoT_SoaringMapTest_ForceAssetsAbsent)
//               the chooser opens on Clock Town in OCARINA_MODE_01; B closes it
//               with OCARINA_MODE_04 and nothing armed. The chooser's crossing
//               path is covered only by XW6's anchors and the shared Depart.
//   MAP CANCEL  #1169 XW8: the owl map opens (pause state 0x101, then the roll-in
//               to SELECT) on Clock Town over exactly the frozen owls; B closes
//               it through SOARING_6, vanilla 0x12 and 0x13 with OCARINA_MODE_04,
//               nothing armed and the HUD back (unk_13EA 0x32).
//   MAP NO      D-right (DPadOnPause) moves to the swamp, A opens "Soar to
//               Southern Swamp?", D-down (DpadInText) to No and A go back to
//               the map; B closes it with the HUD back.
//   CROSS       D-right, A, A on OK arms the one-shot target (OoT, this
//               entrance, MM, the swamp owl's uncleared entrance, the Soaring
//               kind) once the map has closed; OoT must not reload any scene
//               before MM takes over, and MM's arrival check ends the run.
// The map's states freeze the world, so no OnPlayerUpdate runs while it is
// open: its legs are driven from the frame start (SoarMainStart), which pumps
// the emulated buttons while the pause state is not 0. Before any crossing,
// the first OoT leg checks every asset the map draws is mounted (the OoT-first
// case: the curated half, before mm.o2r).
//
// The adult leg flips gSaveContext.linkAge for the notes only, in a scene loaded
// as a child: reloading a scene as an adult swaps the age equipment of a file
// that never had an adult bucket (OoT_Play_Destroy), the crash class the forced
// child arrival guards. Every wait has a frame budget and fails by name.

extern "C" {
u8 OoT_Message_GetState(MessageContext* msgCtx);
bool OoT_SongOfSoaringOoTOn(void);
bool OoT_SoaringOoTOwned(void);
// games/mm/2s2h/GameExports_SingleExe.cpp: MM's frozen half carries what the
// MM leg set (owls, cleared Woodfall, the owl-save pair), read in MM's TU.
int MM_SoaringTest_FrozenHalfReady(char* why, size_t cap);
// games/oot/soh/Enhancements/randomizer/SoaringMapOoT.c and SongOfSoaringOoT.cpp
// (#1169 XW8): the owl map's picker as it stands, the probe override, the probe.
void OoT_SoaringMapTest_Peek(int* phase, int* cursor, uint16_t* mask);
void OoT_SoaringMapTest_ForceAssetsAbsent(int absent);
bool OoT_SoaringMap_AssetsPresent(void);
bool OoT_SoaringMap_IsOpen(void);
}

namespace {

enum class SoarLeg { Adult, NoStatues, Restricted, ChooserCancel, MapCancel, MapNo, Cross };

enum class SoarStep {
    Prepare, // the leg's setup (the Windmill: its door and the landing)
    TakeOut, // C-left until the ocarina is open (MSGMODE_OCARINA_PLAYING)
    Settle,  // a few frames with the ocarina open
    Notes,   // the six notes
    After,   // the leg's own verdict, up to the press that ends it
    Close,   // the box closes and Link has control again
};

const SoarLeg kSoarPreLegs[] = { SoarLeg::Adult, SoarLeg::NoStatues, SoarLeg::Restricted };
const SoarLeg kSoarReturnLegs[] = { SoarLeg::ChooserCancel, SoarLeg::MapCancel, SoarLeg::MapNo, SoarLeg::Cross };
const int kSoarPreLegCount = (int)(sizeof(kSoarPreLegs) / sizeof(kSoarPreLegs[0]));
const int kSoarReturnLegCount = (int)(sizeof(kSoarReturnLegs) / sizeof(kSoarReturnLegs[0]));

// The song: C-down, C-left, C-up, twice (sOcarinaF4BtnMap, sOcarinaB4BtnMap and
// sOcarinaD5BtnMap give OoT's note values 5, 11 and 14, the ones
// Combo_SoaringOoTPitches lists).
const u16 kSoarNotes[RSBS_SOARING_NOTE_COUNT] = { BTN_CDOWN, BTN_CLEFT, BTN_CUP, BTN_CDOWN, BTN_CLEFT, BTN_CUP };
const int kSoarNoteHold = 4;
const int kSoarNoteGap = 4;
const int kSoarNotePeriod = kSoarNoteHold + kSoarNoteGap;
// MM's swamp owl (OWL_WARP_SOUTHERN_SWAMP; MM's TU static-asserts it is 7), and
// the two owls the MM leg activates. The chooser opens on Clock Town, so the
// swamp is the owl "Next statue" moves to.
const int kSoarTargetOwl = 7;
const uint16_t kSoarOwlsActivated = (uint16_t)((1u << RSBS_SOARING_OWL_CLOCK_TOWN) | (1u << kSoarTargetOwl));
// The Windmill (spawn 1, its Kakariko door): SCENE_WINDMILL_AND_DAMPES_GRAVE's
// warp-song restriction is 3 (z_parameter.c, flags2 0x03), the one the
// randomizer lets vanilla warp songs through and Soaring does not (ruling 5).
// Not the grave half of the scene: Dampe's ghost (En_Po_Relay) auto-starts a
// conversation within 250 units. As a child the Windmill's man (En_Fu,
// EnFu_WaitChild) only offers a talk within 100 units and never marks Link as
// near an ocarina actor, but the Song of Storms tag does (SoarQuietWindmillTag).
const uint16_t kSoarRestrictedEntrance = ENTR_WINDMILL_AND_DAMPES_GRAVE_WINDMILL;
const u16 kSoarTextRestricted = 0x88C; // OoT's "You can't warp here!"

// Frame budgets: each wait that runs out fails the row by name.
const int kSoarTakeOutBudget = 300;
const int kSoarSettleFrames = 8;
const int kSoarTextBudget = 900;
const int kSoarCloseBudget = 300;
const int kSoarSceneBudget = 400;
const int kSoarLandBudget = 300;
const int kSoarCursorBudget = 60;
const int kSoarArmBudget = 60;
const int kSoarMapOpenBudget = 120;
const int kSoarDepartBudget = 600;
const int kSoarAdultStayFrames = 30;
const int kSoarIdleFrames = 10;

struct SoarState {
    GameplayPhase phase = GP_PHASE_DONE;
    int leg = 0;
    SoarStep step = SoarStep::Prepare;
    int stepFrames = 0;
    int sub = 0;
    int subFrames = 0;
    int lastPress = -1000;
    int idleFrames = 0;
    bool failed = false;
    bool adultFlipped = false;
    bool sawPlayed = false;
    bool sawMode04 = false;
    bool sawLeave = false;
    bool awaitScene = false;
    bool arrivedScene = false;
    bool awaitArm = false;
    int armFrames = 0;
    bool departed = false;
    int departFrames = 0;
    u16 held = 0;
    int heldFrames = 0;
    // #1169 XW8: the owl map's legs (SoarMapFrame).
    bool mapActive = false;
    int mapSub = 0;
    int mapFrames = 0;
    uint32_t mapSeen = 0;
};
SoarState sSoar;

const char* SoarLegName(SoarLeg leg) {
    switch (leg) {
        case SoarLeg::Adult:
            return "adult refused";
        case SoarLeg::NoStatues:
            return "no statues";
        case SoarLeg::Restricted:
            return "restriction refused";
        case SoarLeg::ChooserCancel:
            return "chooser cancel (the fallback)";
        case SoarLeg::MapCancel:
            return "map cancel";
        case SoarLeg::MapNo:
            return "map No then B";
        case SoarLeg::Cross:
            return "the crossing";
    }
    return "?";
}

SoarLeg SoarCurrentLeg(void) {
    if (sSoar.phase == GP_PHASE_OOT_RETURN) {
        return kSoarReturnLegs[sSoar.leg < kSoarReturnLegCount ? sSoar.leg : kSoarReturnLegCount - 1];
    }
    return kSoarPreLegs[sSoar.leg < kSoarPreLegCount ? sSoar.leg : kSoarPreLegCount - 1];
}

int SoarLegCount(void) {
    return sSoar.phase == GP_PHASE_OOT_RETURN ? kSoarReturnLegCount : kSoarPreLegCount;
}

// The legs the owl map runs (#1169 XW8).
bool SoarIsMapLeg(SoarLeg leg) {
    return leg == SoarLeg::MapCancel || leg == SoarLeg::MapNo || leg == SoarLeg::Cross;
}

void SoarFail(const char* why) {
    if (sSoar.failed) {
        return;
    }
    sSoar.failed = true;
    sSoar.awaitArm = false;
    OoT_SoaringMapTest_ForceAssetsAbsent(0);
    if (sSoar.adultFlipped) {
        gSaveContext.linkAge = LINK_AGE_CHILD;
        sSoar.adultFlipped = false;
    }
    PlayState* play = OoT_gPlayState;
    MessageContext* m = play != NULL ? &play->msgCtx : NULL;
    char msg[1024];
    snprintf(msg, sizeof(msg),
             "int-soaring-to-owl, leg '%s': %s [msgMode=%d textId=0x%04X textState=%d ocarinaMode=%d "
             "ocarinaAction=%d choiceIndex=%d warpArmed=%d owls=0x%03X scene=%d entrance=0x%04X linkAge=%d]",
             SoarLegName(SoarCurrentLeg()), why, m != NULL ? (int)m->msgMode : -1, m != NULL ? (unsigned)m->textId : 0u,
             m != NULL ? (int)OoT_Message_GetState(m) : -1, m != NULL ? (int)m->ocarinaMode : -1,
             m != NULL ? (int)m->ocarinaAction : -1, m != NULL ? (int)m->choiceIndex : -1, ComboWarp_IsArmed() ? 1 : 0,
             (unsigned)Combo_SoaringOwlMask(), play != NULL ? (int)play->sceneNum : -1,
             (unsigned)(uint16_t)gSaveContext.entranceIndex, (int)gSaveContext.linkAge);
    IntegrationTest_GameplayFail(msg);
}

// Holds @p buttons for the next @p frames frames (a fresh press on the first).
void SoarHold(u16 buttons, int frames) {
    sSoar.held = buttons;
    sSoar.heldFrames = frames;
    sSoar.lastPress = sSoar.stepFrames;
}

// Once per driven frame: the emulated buttons are consumed by the next
// PadMgr_ProcessInputs, so a held button is set again every frame.
void SoarPump(void) {
    if (sSoar.heldFrames > 0) {
        GameInteractor_SetEmulatedButtons(GameInteractor_GetEmulatedButtons() | sSoar.held);
        sSoar.heldFrames--;
    }
}

bool SoarIdle(PlayState* play) {
    Player* player = GET_PLAYER(play);
    return player != NULL && play->msgCtx.msgMode == MSGMODE_NONE && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           play->transitionMode == TRANS_MODE_OFF && play->csCtx.state == CS_STATE_IDLE &&
           (player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING)) == 0 &&
           gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void SoarNext(SoarStep step) {
    sSoar.step = step;
    sSoar.stepFrames = 0;
    sSoar.sub = 0;
    sSoar.subFrames = 0;
    sSoar.lastPress = -1000;
    sSoar.idleFrames = 0;
}

void SoarBeginLeg(void) {
    SoarNext(SoarStep::Prepare);
    sSoar.sawPlayed = false;
    sSoar.sawMode04 = false;
    sSoar.sawLeave = false;
    fprintf(stderr, "[PFC] soaring leg '%s' starts (scene %d, owls offered 0x%03X)\n", SoarLegName(SoarCurrentLeg()),
            OoT_gPlayState != NULL ? (int)OoT_gPlayState->sceneNum : -1, (unsigned)Combo_SoaringOwlMask());
    fflush(stderr);
}

void SoarLegPassed(const char* what) {
    fprintf(stderr, "[PFC] soaring leg '%s' PASS: %s\n", SoarLegName(SoarCurrentLeg()), what);
    fflush(stderr);
    OoT_SoaringMapTest_ForceAssetsAbsent(0);
    sSoar.mapActive = false;
    sSoar.leg++;
    if (sSoar.leg < SoarLegCount()) {
        SoarBeginLeg();
    }
}

/**
 * Each phase's setup, once: the song owned (its RAND_INF flag, what its get-item
 * sets), the Fairy Ocarina in its slot and on C-left, and the gates the legs
 * rely on checked up front, so a leg that fails names its own step and not a
 * missing precondition.
 */
bool SoarPreparePhase(PlayState* play, GameplayPhase phase) {
    char why[384];
    Flags_SetRandomizerInf(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT);
    const u8 ocarina = gSaveContext.inventory.items[SLOT_OCARINA];
    if (ocarina != ITEM_OCARINA_FAIRY && ocarina != ITEM_OCARINA_TIME) {
        gSaveContext.inventory.items[SLOT_OCARINA] = ITEM_OCARINA_FAIRY;
    }
    gSaveContext.equips.buttonItems[1] = gSaveContext.inventory.items[SLOT_OCARINA];
    gSaveContext.equips.cButtonSlots[0] = SLOT_OCARINA;
    const uint16_t owls = Combo_SoaringOwlMask();
    if (!OoT_SongOfSoaringOoTOn() || !Combo_ForeignPairingActive() || !OoT_SoaringOoTOwned() || !LINK_IS_CHILD ||
        !Combo_Soaring_MMDescRegistered() || !Context_HasFrozenState(GAME_MM)) {
        snprintf(why, sizeof(why),
                 "a precondition does not hold before the legs (songSoaringOot frozen on %d, paired %d, owned %d, "
                 "child %d, MM layout registered %d, MM half frozen %d)",
                 OoT_SongOfSoaringOoTOn() ? 1 : 0, Combo_ForeignPairingActive() ? 1 : 0, OoT_SoaringOoTOwned() ? 1 : 0,
                 LINK_IS_CHILD ? 1 : 0, Combo_Soaring_MMDescRegistered() ? 1 : 0,
                 Context_HasFrozenState(GAME_MM) ? 1 : 0);
        SoarFail(why);
        return false;
    }
    if (phase == GP_PHASE_OOT_PRE && owls != 0) {
        snprintf(why, sizeof(why), "the creation shadow already offers owls 0x%03X; the no-statues leg needs none",
                 (unsigned)owls);
        SoarFail(why);
        return false;
    }
    // #1169 XW8: before Majora's Mask has ever run, every MM texture the owl
    // map draws is mounted (the curated half), or the map would fall back.
    if (phase == GP_PHASE_OOT_PRE && !OoT_SoaringMap_AssetsPresent()) {
        SoarFail("an owl-map asset is not mounted before the first crossing (the curated half redship-oot.o2r lacks "
                 "it): an OoT-first session would get the text chooser instead of the map");
        return false;
    }
    if (phase == GP_PHASE_OOT_RETURN) {
        // Exactly these two: "Next statue" walks upward from Clock Town, so an
        // extra owl in 5..6 would take the step and the Cross leg would red on
        // the wrong production code.
        if ((owls & RSBS_SOARING_OWL_BITS) != kSoarOwlsActivated) {
            snprintf(why, sizeof(why),
                     "MM's frozen half offers owls 0x%03X after the MM leg set exactly 0x%03X (the departure did not "
                     "freeze them, the half is on day 0, or another owl is active)",
                     (unsigned)owls, (unsigned)kSoarOwlsActivated);
            SoarFail(why);
            return false;
        }
        // The half the arrival consumes must be the one the MM leg prepared, or
        // the cleared-variant and pause-save assertions would prove nothing.
        char half[256];
        if (MM_SoaringTest_FrozenHalfReady(half, sizeof(half)) == 0) {
            snprintf(why, sizeof(why), "the MM leg's preparation did not survive the departure's freeze: %s", half);
            SoarFail(why);
            return false;
        }
        fprintf(stderr, "[PFC] soaring: %s\n", half);
        // The chooser's cursor moves with the D-pad when this is on (the stick
        // cannot be emulated); in memory for the run, put back at the verdict.
        IntegrationTest_SoaringSetCVarInt(CVAR_SETTING("DpadInText"), 1);
        // #1169 XW8: the owl map's cursor moves with the D-pad under SoH's
        // DPadOnPause (the prompt's choice under DpadInText above).
        IntegrationTest_SoaringSetCVarInt(CVAR_SETTING("DPadOnPause"), 1);
    }
    fprintf(stderr,
            "[PFC] soaring: %s legs on scene %d; song owned, Fairy Ocarina on C-left, rule frozen on, owls offered "
            "0x%03X\n",
            phase == GP_PHASE_OOT_PRE ? "first OoT" : "return", (int)play->sceneNum, (unsigned)owls);
    fflush(stderr);
    return true;
}

// The Windmill's Song of Storms spot (EnOkarinaTag type 2, the child-only tag
// that drains Kakariko's well; z_en_okarina_tag.c). Within 50 + its range of
// it, a child taking out the ocarina plays for the tag (PLAYER_STATE2_NEAR_
// OCARINA_ACTOR, then OCARINA_ACTION_CHECK_STORMS), not in free play, and no
// warp song (vanilla or Soaring) is ever heard there: the first real run stood
// at the Kakariko door inside its range and failed with ocarinaAction 0x27. A
// player who has drained the well finds the tag quiet (its switch flag set,
// VB_OKARINA_TAG_COMPLETED), and that is the state the leg plays in: the
// switch is set here, in the live scene, and put back before the Windmill is
// left (SoarPreLegsDone).
const s16 kSoarWindmillTagType = 2;
s16 sSoarWindmillTagSwitch = -1;

bool SoarQuietWindmillTag(PlayState* play) {
    Player* player = GET_PLAYER(play);
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = actor->next) {
        if (actor->id != ACTOR_EN_OKARINA_TAG || actor->update == NULL) {
            continue;
        }
        const s16 type = (actor->params >> 0xA) & 0x3F;
        const s16 sw = actor->params & 0x3F;
        const f32 range = actor->home.rot.z > 0 ? actor->home.rot.z * 40.0f : 0.0f;
        fprintf(stderr,
                "[PFC] soaring leg 'restriction refused': ocarina tag type %d, switch 0x%02X, range %.0f, Link %.0f "
                "units away (pos %.0f %.0f %.0f)\n",
                (int)type, (unsigned)sw, range, actor->xzDistToPlayer, player->actor.world.pos.x,
                player->actor.world.pos.y, player->actor.world.pos.z);
        fflush(stderr);
        if (type != kSoarWindmillTagType) {
            continue;
        }
        if (sw == 0x3F) {
            SoarFail("the Windmill's Song of Storms tag has no switch flag, so no drained-well state quiets it: a "
                     "child at the Kakariko door can never play in free play there");
            return false;
        }
        if (!OoT_Flags_GetSwitch(play, sw)) {
            OoT_Flags_SetSwitch(play, sw);
            sSoarWindmillTagSwitch = sw;
            fprintf(stderr,
                    "[PFC] soaring leg 'restriction refused': the well is drained (switch 0x%02X set), so the "
                    "Song of Storms tag is quiet and the ocarina plays in free play\n",
                    (unsigned)sw);
            fflush(stderr);
        }
    }
    return true;
}

// The leg's setup. Only the Windmill needs one: its door, the scene, a landing.
void SoarStepPrepare(PlayState* play) {
    // #1169 XW8: the chooser leg runs the owl map's fallback route.
    OoT_SoaringMapTest_ForceAssetsAbsent(SoarCurrentLeg() == SoarLeg::ChooserCancel ? 1 : 0);
    if (SoarCurrentLeg() != SoarLeg::Restricted) {
        SoarNext(SoarStep::TakeOut);
        return;
    }
    char why[192];
    sSoar.subFrames++;
    switch (sSoar.sub) {
        case 0:
            if (!SoarIdle(play)) {
                if (sSoar.subFrames > kSoarCloseBudget) {
                    SoarFail("Link never stood idle in the Market before the Windmill door");
                }
                return;
            }
            sSoar.awaitScene = true;
            sSoar.arrivedScene = false;
            GpFireOoTDoor(kSoarRestrictedEntrance, "the Windmill door (warp-song restriction 3)");
            sSoar.sub = 1;
            sSoar.subFrames = 0;
            return;
        case 1:
            if (sSoar.arrivedScene) {
                sSoar.sub = 2;
                sSoar.subFrames = 0;
                sSoar.idleFrames = 0;
            } else if (sSoar.subFrames > kSoarSceneBudget) {
                SoarFail("the Windmill never loaded after its door was fired");
            }
            return;
        default:
            sSoar.idleFrames = SoarIdle(play) ? sSoar.idleFrames + 1 : 0;
            if (sSoar.idleFrames < 20) {
                if (sSoar.subFrames > kSoarLandBudget) {
                    SoarFail("Link never stood idle in the Windmill after the door");
                }
                return;
            }
            if (play->sceneNum != SCENE_WINDMILL_AND_DAMPES_GRAVE || play->interfaceCtx.restrictions.warpSongs != 3) {
                snprintf(why, sizeof(why), "the Windmill is scene %d with warp-song restriction %d; the leg needs 3",
                         (int)play->sceneNum, (int)play->interfaceCtx.restrictions.warpSongs);
                SoarFail(why);
                return;
            }
            if (!SoarQuietWindmillTag(play)) {
                return;
            }
            SoarNext(SoarStep::TakeOut);
            return;
    }
}

void SoarStepTakeOut(PlayState* play) {
    if (play->msgCtx.msgMode == MSGMODE_OCARINA_PLAYING) {
        SoarNext(SoarStep::Settle);
        return;
    }
    if (sSoar.stepFrames > kSoarTakeOutBudget) {
        SoarFail("Link never took out the ocarina: no MSGMODE_OCARINA_PLAYING after C-left presses (the Fairy "
                 "Ocarina is on C-left)");
        return;
    }
    if (SoarIdle(play) && sSoar.heldFrames == 0 && sSoar.stepFrames - sSoar.lastPress >= 30) {
        SoarHold(BTN_CLEFT, 2);
        return;
    }
    // A textbox nobody else will press past (the paired drive's own frame-30
    // pickup, say) would keep Link from ever standing idle: A, once it has
    // printed. Never the Soaring texts, which only a played song opens.
    const u8 state = OoT_Message_GetState(&play->msgCtx);
    const u16 id = play->msgCtx.textId;
    if (play->msgCtx.msgMode == MSGMODE_TEXT_DONE && (state == TEXT_STATE_DONE || state == TEXT_STATE_DONE_HAS_NEXT) &&
        id != RSBS_OOT_TEXT_SOARING_PLAYED && id != RSBS_OOT_TEXT_SOARING_NO_STATUES &&
        id != RSBS_OOT_TEXT_SOARING_CHOOSER && id != RSBS_OOT_TEXT_SOARING_CONFIRM && sSoar.heldFrames == 0 &&
        sSoar.stepFrames - sSoar.lastPress >= 15) {
        fprintf(stderr, "[PFC] soaring leg '%s': pressing A past text 0x%04X before taking out the ocarina\n",
                SoarLegName(SoarCurrentLeg()), (unsigned)id);
        fflush(stderr);
        SoarHold(BTN_A, 1);
    }
}

void SoarStepSettle(PlayState* play) {
    if (play->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING) {
        SoarFail("the ocarina closed before the first note");
        return;
    }
    if (sSoar.stepFrames < kSoarSettleFrames) {
        return;
    }
    if (SoarCurrentLeg() == SoarLeg::Adult) {
        // For the notes only (see the block comment): the age the matcher reads.
        gSaveContext.linkAge = LINK_AGE_ADULT;
        sSoar.adultFlipped = true;
    }
    SoarNext(SoarStep::Notes);
}

void SoarStepNotes(PlayState* play) {
    const int t = sSoar.stepFrames - 1;
    const int note = t / kSoarNotePeriod;
    if (note >= (int)RSBS_SOARING_NOTE_COUNT) {
        fprintf(stderr, "[PFC] soaring leg '%s': six notes played (C-down C-left C-up, twice)\n",
                SoarLegName(SoarCurrentLeg()));
        fflush(stderr);
        SoarNext(SoarStep::After);
        return;
    }
    if (t > 0 && note < (int)RSBS_SOARING_NOTE_COUNT - 1 && play->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING) {
        char why[160];
        snprintf(why, sizeof(why), "the ocarina left MSGMODE_OCARINA_PLAYING after note %d of 6, before the sixth",
                 note + 1);
        SoarFail(why);
        return;
    }
    if (t % kSoarNotePeriod == 0) {
        SoarHold(kSoarNotes[note], kSoarNoteHold);
    }
}

// The texts a refusal leg must not show, and the chooser, which none may open.
bool SoarRefusalTextsWrong(PlayState* play, u16 want) {
    const u16 id = play->msgCtx.textId;
    char why[192];
    if (id == RSBS_OOT_TEXT_SOARING_CHOOSER) {
        SoarFail("the owl chooser opened: the gates offered owls where they must refuse");
        return true;
    }
    if ((id == kSoarTextRestricted || id == RSBS_OOT_TEXT_SOARING_NO_STATUES) && id != want) {
        snprintf(why, sizeof(why), "the refusal showed text 0x%04X, not 0x%04X", (unsigned)id, (unsigned)want);
        SoarFail(why);
        return true;
    }
    return false;
}

// Before the song matched (or failed to): the notes must have matched within
// the budget, which "You played the Song of Soaring." (E3) proves.
bool SoarNotMatched(PlayState* play) {
    if (!sSoar.sawPlayed && sSoar.stepFrames > 90 && play->msgCtx.msgMode == MSGMODE_OCARINA_PLAYING) {
        char why[256];
        snprintf(why, sizeof(why),
                 "the six notes did not match: still MSGMODE_OCARINA_PLAYING 90 frames after them (rule %d, paired "
                 "%d, owned %d, child %d, free play %d)",
                 OoT_SongOfSoaringOoTOn() ? 1 : 0, Combo_ForeignPairingActive() ? 1 : 0, OoT_SoaringOoTOwned() ? 1 : 0,
                 LINK_IS_CHILD ? 1 : 0, play->msgCtx.ocarinaAction == OCARINA_ACTION_FREE_PLAY ? 1 : 0);
        SoarFail(why);
        return true;
    }
    return false;
}

void SoarStepAfter(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    const SoarLeg leg = SoarCurrentLeg();
    char why[256];
    if (m->textId == RSBS_OOT_TEXT_SOARING_PLAYED) {
        sSoar.sawPlayed = true;
    }
    if (m->ocarinaMode == OCARINA_MODE_04) {
        sSoar.sawMode04 = true;
    }
    if (leg != SoarLeg::Cross && ComboWarp_IsArmed()) {
        SoarFail("the one-shot warp target is armed");
        return;
    }
    switch (leg) {
        case SoarLeg::Adult:
            if (m->msgMode != MSGMODE_OCARINA_PLAYING || m->lastPlayedSong == RSBS_OOT_SONG_SOARING ||
                sSoar.sawPlayed) {
                snprintf(why, sizeof(why),
                         "an adult's notes left MSGMODE_OCARINA_PLAYING (msgMode %d, lastPlayedSong 0x%X): the song "
                         "matched, or the ocarina failed; it must stay open until B",
                         (int)m->msgMode, (unsigned)m->lastPlayedSong);
                SoarFail(why);
                return;
            }
            if (sSoar.stepFrames < kSoarAdultStayFrames) {
                return;
            }
            gSaveContext.linkAge = LINK_AGE_CHILD;
            sSoar.adultFlipped = false;
            fprintf(stderr,
                    "[PFC] soaring leg 'adult refused': still MSGMODE_OCARINA_PLAYING %d frames after the notes; "
                    "pressing B\n",
                    kSoarAdultStayFrames);
            fflush(stderr);
            SoarHold(BTN_B, 2);
            SoarNext(SoarStep::Close);
            return;
        case SoarLeg::NoStatues:
        case SoarLeg::Restricted: {
            const u16 want = leg == SoarLeg::NoStatues ? (u16)RSBS_OOT_TEXT_SOARING_NO_STATUES : kSoarTextRestricted;
            if (SoarRefusalTextsWrong(play, want) || SoarNotMatched(play)) {
                return;
            }
            if (m->textId == want) {
                if (!sSoar.sawPlayed) {
                    SoarFail("the refusal showed without \"You played the Song of Soaring.\" before it (E3)");
                    return;
                }
                fprintf(stderr, "[PFC] soaring leg '%s': \"You played the Song of Soaring.\", then text 0x%04X\n",
                        SoarLegName(leg), (unsigned)want);
                fflush(stderr);
                SoarNext(SoarStep::Close);
                return;
            }
            if (sSoar.stepFrames > kSoarTextBudget) {
                snprintf(why, sizeof(why), "text 0x%04X never showed after the notes (\"You played\" seen %d)",
                         (unsigned)want, sSoar.sawPlayed ? 1 : 0);
                SoarFail(why);
            }
            return;
        }
        case SoarLeg::ChooserCancel:
            break;
        case SoarLeg::MapCancel:
        case SoarLeg::MapNo:
        case SoarLeg::Cross:
            // #1169 XW8: until the map's pause opens (then SoarMainStart drives).
            if (m->textId == kSoarTextRestricted || m->textId == RSBS_OOT_TEXT_SOARING_NO_STATUES) {
                snprintf(why, sizeof(why), "the song was refused with text 0x%04X while MM's half offers owls 0x%03X",
                         (unsigned)m->textId, (unsigned)Combo_SoaringOwlMask());
                SoarFail(why);
                return;
            }
            if (m->textId == RSBS_OOT_TEXT_SOARING_CHOOSER) {
                SoarFail("the song opened XW6's text chooser, not the owl map (an asset is not mounted, the pause "
                         "precondition refused, or the route fell back)");
                return;
            }
            if (SoarNotMatched(play)) {
                return;
            }
            if (sSoar.stepFrames > kSoarTextBudget) {
                snprintf(why, sizeof(why),
                         "the owl map never opened after the notes (pause state %u, \"You played\" seen %d)",
                         (unsigned)play->pauseCtx.state, sSoar.sawPlayed ? 1 : 0);
                SoarFail(why);
            }
            return;
    }

    // The fallback chooser leg.
    if (m->textId == kSoarTextRestricted || m->textId == RSBS_OOT_TEXT_SOARING_NO_STATUES) {
        snprintf(why, sizeof(why), "the song was refused with text 0x%04X while MM's half offers owls 0x%03X",
                 (unsigned)m->textId, (unsigned)Combo_SoaringOwlMask());
        SoarFail(why);
        return;
    }
    if (SoarNotMatched(play)) {
        return;
    }
    const bool choosing = m->textId == RSBS_OOT_TEXT_SOARING_CHOOSER && OoT_Message_GetState(m) == TEXT_STATE_CHOICE;
    sSoar.subFrames++;
    if (sSoar.sub == 0) {
        if (!choosing) {
            if (sSoar.stepFrames > kSoarTextBudget) {
                snprintf(why, sizeof(why),
                         "the owl chooser (text 0x%04X) never reached TEXT_STATE_CHOICE after the notes (\"You "
                         "played\" seen %d)",
                         (unsigned)RSBS_OOT_TEXT_SOARING_CHOOSER, sSoar.sawPlayed ? 1 : 0);
                SoarFail(why);
            }
            return;
        }
        if (!sSoar.sawPlayed || m->ocarinaMode != OCARINA_MODE_01 || m->choiceIndex != 0) {
            snprintf(why, sizeof(why),
                     "the chooser opened wrong (\"You played\" seen %d, ocarinaMode %d, not OCARINA_MODE_01; cursor "
                     "on choice %d, not Soar)",
                     sSoar.sawPlayed ? 1 : 0, (int)m->ocarinaMode, (int)m->choiceIndex);
            SoarFail(why);
            return;
        }
        fprintf(stderr, "[PFC] soaring leg '%s': the owl chooser is open in OCARINA_MODE_01, cursor on Soar\n",
                SoarLegName(leg));
        fflush(stderr);
        SoarHold(BTN_B, 2);
        SoarNext(SoarStep::Close);
        return;
    }
}

/**
 * #1169 XW8: one frame of an owl-map leg, from the frame start (the map's
 * states freeze the world: no OnPlayerUpdate runs until it closes). Reads the
 * pause state and the picker (OoT_SoaringMapTest_Peek) the map leaves.
 */
uint32_t SoarMapStateBit(uint16_t st) {
    switch (st) {
        case RSBS_OOT_PAUSE_SOARING_0:
            return 1u << 0;
        case RSBS_OOT_PAUSE_SOARING_1:
            return 1u << 1;
        case RSBS_OOT_PAUSE_SOARING_2:
            return 1u << 2;
        case RSBS_OOT_PAUSE_SOARING_3:
            return 1u << 3;
        case RSBS_OOT_PAUSE_SOARING_SELECT:
            return 1u << 4;
        case RSBS_OOT_PAUSE_SOARING_CONFIRM:
            return 1u << 5;
        case RSBS_OOT_PAUSE_SOARING_6:
            return 1u << 6;
        case RSBS_OOT_PAUSE_UNPAUSE_SETUP:
            return 1u << 7;
        case RSBS_OOT_PAUSE_UNPAUSE_CLOSE:
            return 1u << 8;
        default:
            return 0;
    }
}

void SoarMapFrame(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    const SoarLeg leg = SoarCurrentLeg();
    const uint16_t st = play->pauseCtx.state;
    char why[384];
    if (!sSoar.mapActive) {
        if (st == 0) {
            return;
        }
        sSoar.mapActive = true;
        sSoar.mapSub = 0;
        sSoar.mapFrames = 0;
        sSoar.mapSeen = 0;
        if (st != RSBS_OOT_PAUSE_SOARING_0) {
            snprintf(why, sizeof(why),
                     "the pause opened in state 0x%X, not the owl map's 0x%X (a vanilla pause, or no map)",
                     (unsigned)st, (unsigned)RSBS_OOT_PAUSE_SOARING_0);
            SoarFail(why);
            return;
        }
        fprintf(stderr, "[PFC] soaring leg '%s': the owl map's pause opened (state 0x%X)\n", SoarLegName(leg),
                (unsigned)st);
        fflush(stderr);
    }
    sSoar.mapSeen |= SoarMapStateBit(st);
    sSoar.mapFrames++;
    int phase = -1;
    int cursor = -1;
    uint16_t mask = 0;
    OoT_SoaringMapTest_Peek(&phase, &cursor, &mask);
    if (st != 0 && st != RSBS_OOT_PAUSE_UNPAUSE_SETUP && st != RSBS_OOT_PAUSE_UNPAUSE_CLOSE &&
        !Combo_SoaringMap_IsMapState(st)) {
        snprintf(why, sizeof(why), "the owl map's pause is in state 0x%X, which is neither the map's nor its close",
                 (unsigned)st);
        SoarFail(why);
        return;
    }
    if (cursor >= 0 && ((mask >> cursor) & 1u) == 0) {
        snprintf(why, sizeof(why), "the map's cursor is on owl %d, which mask 0x%03X does not offer", cursor,
                 (unsigned)mask);
        SoarFail(why);
        return;
    }
    switch (sSoar.mapSub) {
        case 0:
            // The roll-in, to SELECT on Clock Town over exactly the frozen owls.
            if (st == RSBS_OOT_PAUSE_SOARING_SELECT) {
                const uint32_t opened =
                    SoarMapStateBit(RSBS_OOT_PAUSE_SOARING_0) | SoarMapStateBit(RSBS_OOT_PAUSE_SOARING_3);
                if ((sSoar.mapSeen & opened) != opened || phase != (int)COMBO_SOARING_MAP_SELECT ||
                    cursor != RSBS_SOARING_OWL_CLOCK_TOWN || mask != kSoarOwlsActivated) {
                    snprintf(why, sizeof(why),
                             "the map opened wrong (states seen 0x%03X, phase %d, cursor %d, not Clock Town, mask "
                             "0x%03X, not 0x%03X)",
                             (unsigned)sSoar.mapSeen, phase, cursor, (unsigned)mask, (unsigned)kSoarOwlsActivated);
                    SoarFail(why);
                    return;
                }
                fprintf(stderr, "[PFC] soaring leg '%s': the map is on Clock Town over owls 0x%03X\n", SoarLegName(leg),
                        (unsigned)mask);
                fflush(stderr);
                if (leg == SoarLeg::MapCancel) {
                    SoarHold(BTN_B, 2);
                    sSoar.mapSub = 10;
                } else {
                    SoarHold(BTN_DRIGHT, 1);
                    sSoar.mapSub = 1;
                }
                sSoar.mapFrames = 0;
            } else if (sSoar.mapFrames > kSoarMapOpenBudget) {
                SoarFail("the owl map never reached its cursor state (SOARING_SELECT) after opening");
            }
            return;
        case 1:
            // D-right: Clock Town (4) -> the swamp (7), the next active owl.
            if (st == RSBS_OOT_PAUSE_SOARING_SELECT && cursor == kSoarTargetOwl) {
                SoarHold(BTN_A, 1);
                sSoar.mapSub = 2;
                sSoar.mapFrames = 0;
            } else if (sSoar.mapFrames > kSoarCursorBudget) {
                SoarFail("D-right (DPadOnPause) never moved the map's cursor to the swamp owl");
            }
            return;
        case 2:
            // A: "Soar to Southern Swamp?" opens and reaches its choice.
            if (st == RSBS_OOT_PAUSE_SOARING_CONFIRM && m->textId == RSBS_OOT_TEXT_SOARING_CONFIRM &&
                OoT_Message_GetState(m) == TEXT_STATE_CHOICE) {
                if (m->choiceIndex != 0 || m->ocarinaMode == OCARINA_MODE_01 || m->ocarinaMode == OCARINA_MODE_02) {
                    snprintf(why, sizeof(why),
                             "the prompt opened on choice %d (not OK) or in ocarinaMode %d (01 or 02 would warp "
                             "through the vanilla close)",
                             (int)m->choiceIndex, (int)m->ocarinaMode);
                    SoarFail(why);
                    return;
                }
                if (leg == SoarLeg::MapNo) {
                    SoarHold(BTN_DDOWN, 1);
                    sSoar.mapSub = 3;
                } else {
                    fprintf(stderr, "[PFC] soaring leg 'the crossing': the map's prompt is on OK; choosing it\n");
                    fflush(stderr);
                    IntegrationTest_PairedMarkCrossing("Majora's Mask");
                    SoarHold(BTN_A, 1);
                    sSoar.awaitArm = true;
                    sSoar.armFrames = 0;
                    sSoar.mapSub = 20;
                }
                sSoar.mapFrames = 0;
            } else if (sSoar.mapFrames > kSoarTextBudget) {
                SoarFail("the map's prompt (text 0x9216) never reached its choice after A");
            }
            return;
        case 3:
            // D-down (DpadInText): No.
            if (m->choiceIndex == 1) {
                SoarHold(BTN_A, 1);
                sSoar.mapSub = 4;
                sSoar.mapFrames = 0;
            } else if (sSoar.mapFrames > kSoarCursorBudget) {
                SoarFail("D-down (DpadInText) never moved the prompt's choice to No");
            }
            return;
        case 4:
            // A on No: back on the map, the swamp still under the cursor.
            if (st == RSBS_OOT_PAUSE_SOARING_SELECT && phase == (int)COMBO_SOARING_MAP_SELECT) {
                if (cursor != kSoarTargetOwl || m->msgLength != 0 || ComboWarp_IsArmed()) {
                    snprintf(why, sizeof(why), "No went back wrong (cursor %d, prompt length %d, warp armed %d)",
                             cursor, (int)m->msgLength, ComboWarp_IsArmed() ? 1 : 0);
                    SoarFail(why);
                    return;
                }
                fprintf(stderr, "[PFC] soaring leg 'map No then B': No went back to the map; pressing B\n");
                fflush(stderr);
                SoarHold(BTN_B, 2);
                sSoar.mapSub = 10;
                sSoar.mapFrames = 0;
            } else if (st != RSBS_OOT_PAUSE_SOARING_CONFIRM) {
                snprintf(why, sizeof(why), "A on No closed the map (pause state 0x%X)", (unsigned)st);
                SoarFail(why);
            } else if (sSoar.mapFrames > kSoarCursorBudget) {
                SoarFail("A on No never went back to the map");
            }
            return;
        case 10:
            // B: the close, through SOARING_6, vanilla 0x12 and 0x13, to 0.
            if (st == 0) {
                const uint32_t closed = SoarMapStateBit(RSBS_OOT_PAUSE_SOARING_6) |
                                        SoarMapStateBit(RSBS_OOT_PAUSE_UNPAUSE_SETUP) |
                                        SoarMapStateBit(RSBS_OOT_PAUSE_UNPAUSE_CLOSE);
                if ((sSoar.mapSeen & closed) != closed || ComboWarp_IsArmed() || m->ocarinaMode != OCARINA_MODE_04 ||
                    gSaveContext.unk_13EA != RSBS_OOT_HUD_RESTORE_ALL || OoT_SoaringMap_IsOpen()) {
                    snprintf(why, sizeof(why),
                             "B closed the map wrong (states seen 0x%03X, want SOARING_6, 0x12 and 0x13; warp armed "
                             "%d; ocarinaMode %d, not 04; HUD unk_13EA 0x%X, not 0x32; map still open %d)",
                             (unsigned)sSoar.mapSeen, ComboWarp_IsArmed() ? 1 : 0, (int)m->ocarinaMode,
                             (unsigned)gSaveContext.unk_13EA, OoT_SoaringMap_IsOpen() ? 1 : 0);
                    SoarFail(why);
                    return;
                }
                fprintf(stderr,
                        "[PFC] soaring leg '%s': the map closed through 0x112, 0x12 and 0x13 with OCARINA_MODE_04, "
                        "nothing armed, the HUD back\n",
                        SoarLegName(leg));
                fflush(stderr);
                sSoar.mapActive = false;
                sSoar.sawMode04 = true;
                SoarNext(SoarStep::Close);
            } else if (sSoar.mapFrames > kSoarCloseBudget) {
                SoarFail("the owl map never closed after B");
            }
            return;
        default:
            // 20: OK pressed; the arm watch below takes the verdict.
            return;
    }
}

void SoarStepClose(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    const SoarLeg leg = SoarCurrentLeg();
    if (m->ocarinaMode == OCARINA_MODE_04) {
        sSoar.sawMode04 = true;
    }
    if (ComboWarp_IsArmed()) {
        SoarFail("the one-shot warp target is armed after the leg ended");
        return;
    }
    // TEXT_CLOSING zeroes textId (z_message_PAL.c), so this only sees an open
    // chooser; the msgMode guard is belt and braces.
    if (m->textId == RSBS_OOT_TEXT_SOARING_CHOOSER && m->msgMode != MSGMODE_NONE && sSoar.stepFrames > 30) {
        SoarFail("the chooser is still open 30 frames after B");
        return;
    }
    // A closes a refusal box once it has printed, and any other box that opens
    // once the ocarina is put away (an actor or Navi talk Player can start after
    // OCARINA_MODE_04), so the leg ends on Link's control, not on a stray box.
    // Never the chooser or "You played", which only the song opens.
    const u8 state = OoT_Message_GetState(m);
    const bool refusalBox = (leg == SoarLeg::NoStatues || leg == SoarLeg::Restricted) && state == TEXT_STATE_DONE;
    const bool strayBox = m->msgMode == MSGMODE_TEXT_DONE &&
                          (state == TEXT_STATE_DONE || state == TEXT_STATE_DONE_HAS_NEXT) &&
                          m->textId != RSBS_OOT_TEXT_SOARING_PLAYED && m->textId != RSBS_OOT_TEXT_SOARING_CHOOSER &&
                          m->textId != RSBS_OOT_TEXT_SOARING_CONFIRM;
    if ((refusalBox || strayBox) && sSoar.heldFrames == 0 && sSoar.stepFrames - sSoar.lastPress >= 15) {
        if (!refusalBox) {
            fprintf(stderr, "[PFC] soaring leg '%s': pressing A past text 0x%04X after the ocarina closed\n",
                    SoarLegName(leg), (unsigned)m->textId);
            fflush(stderr);
        }
        SoarHold(BTN_A, 1);
    }
    // Control back: no box, and Link free to act (SoarIdle), which also proves
    // the restriction leg's hand-back before the Happy Mask Shop door.
    sSoar.idleFrames = SoarIdle(play) ? sSoar.idleFrames + 1 : 0;
    if (sSoar.idleFrames >= kSoarIdleFrames) {
        if (!sSoar.sawMode04) {
            SoarFail("control came back but OCARINA_MODE_04 was never seen");
            return;
        }
        SoarLegPassed(
            leg == SoarLeg::Adult           ? "nothing matched, the ocarina stayed open until B, then "
                                              "OCARINA_MODE_04 and control back, nothing armed"
            : leg == SoarLeg::ChooserCancel ? "the fallback chooser opened in OCARINA_MODE_01; B closed it with "
                                              "OCARINA_MODE_04, nothing armed, control back"
            : leg == SoarLeg::MapCancel     ? "the owl map opened on Clock Town; B closed it with OCARINA_MODE_04, "
                                              "nothing armed, the HUD and control back"
            : leg == SoarLeg::MapNo         ? "No went back to the map and B closed it with OCARINA_MODE_04, "
                                              "nothing armed, the HUD and control back"
            : leg == SoarLeg::NoStatues     ? "the no-statues text, OCARINA_MODE_04, nothing armed, control back"
                                            : "0x88C, OCARINA_MODE_04, nothing armed, control back");
        return;
    }
    if (sSoar.stepFrames > kSoarCloseBudget) {
        SoarFail(m->msgMode == MSGMODE_NONE ? "the box closed but Link never stood idle (SoarIdle)"
                                            : "the box never closed and control never came back");
    }
}

// The first OoT leg is over: the round trip's own door, exactly as the paired
// drive fires it.
void SoarPreLegsDone(void) {
    // The adult flip must be undone before the door reloads a scene (a reload
    // with linkAge != linkAgeOnLoad swaps the age equipment, OoT_Play_Destroy).
    if (sSoar.adultFlipped || gSaveContext.linkAge != LINK_AGE_CHILD) {
        SoarFail("Link is not a child at the end of the first OoT leg (the adult leg's age flip was not undone)");
        return;
    }
    // The world as it was: the drained-well switch goes back before the door.
    if (sSoarWindmillTagSwitch >= 0 && OoT_gPlayState != NULL) {
        OoT_Flags_UnsetSwitch(OoT_gPlayState, sSoarWindmillTagSwitch);
        sSoarWindmillTagSwitch = -1;
    }
    gSaveContext.deaths = kPfcDeathsSentinel;
    fprintf(stderr, "[PFC] soaring: the first OoT leg's three refusals PASS; OoT-half sentinel deaths=%u\n",
            (unsigned)kPfcDeathsSentinel);
    IntegrationTest_PairedMarkCrossing("Majora's Mask");
    GpFireOoTDoor(OOT_ENTR_HAPPY_MASK_SHOP, "Happy Mask Shop door (after the Soaring refusals)");
    IntegrationTest_SetGameplayPhase(GP_PHASE_MM_STABILIZE);
}

/** Per driven OoT frame of GP_PHASE_OOT_PRE / GP_PHASE_OOT_RETURN, once the
 *  paired drive's own frames for the phase are done. */
void SoarFrame(PlayState* play, GameplayPhase phase) {
    if (sSoar.failed || play == NULL) {
        return;
    }
    if (sSoar.phase != phase) {
        sSoar.phase = phase;
        sSoar.leg = 0;
        if (!SoarPreparePhase(play, phase)) {
            return;
        }
        SoarBeginLeg();
    }
    if (sSoar.leg >= SoarLegCount()) {
        return;
    }
    SoarPump();
    sSoar.stepFrames++;
    switch (sSoar.step) {
        case SoarStep::Prepare:
            SoarStepPrepare(play);
            break;
        case SoarStep::TakeOut:
            SoarStepTakeOut(play);
            break;
        case SoarStep::Settle:
            SoarStepSettle(play);
            break;
        case SoarStep::Notes:
            SoarStepNotes(play);
            break;
        case SoarStep::After:
            SoarStepAfter(play);
            break;
        case SoarStep::Close:
            SoarStepClose(play);
            break;
    }
    if (!sSoar.failed && phase == GP_PHASE_OOT_PRE && sSoar.leg >= SoarLegCount()) {
        SoarPreLegsDone();
    }
}

/**
 * OoT's frame start, every frame (OnGameStateMainStart): the Soar verdict, and
 * the watch over the fade. From the departure on Link is frozen and runs no
 * OnPlayerUpdate, so this is where the drive sees it.
 */
void SoarMainStart(void) {
    // A failure from another path (the watchdog, a paired check) inside the
    // adult window: put the age back before anything reloads or destroys the
    // child-loaded scene (SoarPlayDestroy is the last line).
    if (sSoar.adultFlipped && IntegrationTest_ExitRequested()) {
        gSaveContext.linkAge = LINK_AGE_CHILD;
        sSoar.adultFlipped = false;
    }
    if (Context_GetCurrentGame() == GAME_MM || sSoar.failed) {
        return;
    }
    char why[384];
    // Only Soar may arm the one-shot target. The departure freezes Link at once
    // (no OnPlayerUpdate, so no step check runs again), and an unplanned
    // crossing lands MM in a phase its tick ignores: the frame start is the one
    // place that still sees a leg that must not soar arming the target.
    if (sSoar.phase != GP_PHASE_DONE && !sSoar.awaitArm && !sSoar.departed && ComboWarp_IsArmed()) {
        SoarFail("the one-shot warp target is armed although Soar was not chosen (an unplanned departure)");
        return;
    }
    // #1169 XW8: the owl map's legs. Its states freeze the world (no
    // OnPlayerUpdate, so no SoarFrame): the buttons are pumped here while the
    // pause state is not 0, and the legs step here until the map has closed.
    PlayState* mapPlay = OoT_gPlayState;
    if (mapPlay != NULL && sSoar.phase == GP_PHASE_OOT_RETURN && sSoar.leg < SoarLegCount() &&
        SoarIsMapLeg(SoarCurrentLeg()) && sSoar.step == SoarStep::After &&
        (mapPlay->pauseCtx.state != 0 || sSoar.mapActive)) {
        if (mapPlay->pauseCtx.state != 0) {
            SoarPump();
        }
        SoarMapFrame(mapPlay);
        if (sSoar.failed) {
            return;
        }
    }
    if (sSoar.awaitArm) {
        sSoar.armFrames++;
        ComboWarpTarget t;
        if (ComboWarp_Peek(&t) && t.armed) {
            sSoar.awaitArm = false;
            const uint16_t want = Combo_SoaringOwlEntrance(kSoarTargetOwl);
            if (t.targetEntrance == Combo_SoaringOwlEntrance(RSBS_SOARING_OWL_CLOCK_TOWN)) {
                SoarFail("Soar armed Clock Town's owl, the first one offered, not the owl \"Next statue\" moved to");
                return;
            }
            if (t.sourceGame != GAME_OOT || t.targetGame != GAME_MM || t.targetEntrance != want ||
                t.kind != (uint8_t)RSBS_SOARING_SWITCH_KIND || t.marker != (uint16_t)gSaveContext.entranceIndex) {
                snprintf(why, sizeof(why),
                         "the armed target is {source %d, marker 0x%04X, target %d, entrance 0x%04X, kind %u}; "
                         "expected {OoT, 0x%04X, MM, 0x%04X (%s), kind %u}",
                         (int)t.sourceGame, (unsigned)t.marker, (int)t.targetGame, (unsigned)t.targetEntrance,
                         (unsigned)t.kind, (unsigned)(uint16_t)gSaveContext.entranceIndex, (unsigned)want,
                         Combo_SoaringOwlName(kSoarTargetOwl), (unsigned)RSBS_SOARING_SWITCH_KIND);
                SoarFail(why);
                return;
            }
            fprintf(stderr,
                    "[PFC] soaring leg 'the crossing': Soar armed {OoT, marker 0x%04X, MM, 0x%04X (%s), the Soaring "
                    "kind}; the fade's end must cross\n",
                    (unsigned)t.marker, (unsigned)t.targetEntrance, Combo_SoaringOwlName(kSoarTargetOwl));
            fflush(stderr);
            sSoar.departed = true;
            sSoar.departFrames = 0;
            IntegrationTest_SoaringExpectArrival(kSoarTargetOwl);
            IntegrationTest_SetGameplayPhase(GP_PHASE_MM_STABILIZE);
        } else if (sSoar.armFrames > kSoarArmBudget) {
            SoarFail("A on Soar armed no warp target");
        }
        return;
    }
    if (sSoar.departed && IntegrationTest_GetGameplayPhase() == GP_PHASE_MM_STABILIZE) {
        sSoar.departFrames++;
        if (sSoar.departFrames > kSoarDepartBudget) {
            snprintf(why, sizeof(why),
                     "OoT is still running %d frames after the Soaring departure (warp target armed %d, a switch "
                     "pending %d): the fade never crossed",
                     sSoar.departFrames, ComboWarp_IsArmed() ? 1 : 0, Combo_IsCrossGameSwitch() ? 1 : 0);
            SoarFail(why);
        }
    }
}

/**
 * OoT's scene init in the soaring drive. True when the drive owned it (the
 * caller returns): the Windmill arrival, or any OoT scene after the
 * departure, which means the armed target was spent before the fade ended (or
 * the crossing was refused) and fails the run.
 */
bool SoarSceneInit(GameplayPhase phase, int16_t sceneNum, uint16_t entrance) {
    char why[256];
    if (sSoar.departed || sSoar.awaitArm) {
        snprintf(why, sizeof(why),
                 "OoT loaded scene %d (entrance 0x%04X) after Soar was chosen: the armed target was spent before the "
                 "fade ended, or the crossing was refused",
                 (int)sceneNum, (unsigned)entrance);
        SoarFail(why);
        return true;
    }
    if (phase == GP_PHASE_OOT_PRE && sSoar.awaitScene) {
        sSoar.awaitScene = false;
        if (entrance != kSoarRestrictedEntrance || sceneNum != SCENE_WINDMILL_AND_DAMPES_GRAVE) {
            snprintf(why, sizeof(why), "the Windmill door arrived at scene %d, entrance 0x%04X", (int)sceneNum,
                     (unsigned)entrance);
            SoarFail(why);
            return true;
        }
        fprintf(stderr, "[PFC] soaring: arrived in the Windmill (scene %d, entrance 0x%04X)\n", (int)sceneNum,
                (unsigned)entrance);
        fflush(stderr);
        sSoar.arrivedScene = true;
        return true;
    }
    return false;
}

/**
 * OnPlayDestroy (the top of OoT_Play_Destroy, before its age-equipment swap):
 * whatever ended the run or reloads the scene, a scene loaded as a child is
 * never destroyed with the adult flip still on.
 */
void SoarPlayDestroy(void) {
    if (sSoar.adultFlipped) {
        gSaveContext.linkAge = LINK_AGE_CHILD;
        sSoar.adultFlipped = false;
        fprintf(stderr, "[PFC] soaring: the adult leg's age flip was undone at OoT_Play_Destroy\n");
        fflush(stderr);
    }
}

} // namespace

/**
 * A pass leaves no file behind (#1169 XW6). The paired rows erase file 3 in
 * their OoT exit arm; this row's pass is signalled from MM's arrival
 * (games/mm/2s2h/GameExports_SingleExe.cpp, MM_SoaringCheckArrival), which
 * calls this before the shutdown. A failure keeps the file for forensics, and
 * the next paired run erases it by its name.
 */
extern "C" void OoT_SoaringTest_ErasePairedFile(void) {
    if (!IntegrationTest_SoaringToOwl() || !IntegrationTest_PairedFirstCrossing() ||
        IntegrationTest_PairedSkipCreation()) {
        return;
    }
    Save_DeleteFile(kPfcSlot);
    fprintf(stderr, "[PFC] PASS: the Song of Soaring crossing; erased file %d\n", kPfcSlot + 1);
    fflush(stderr);
}

/**
 * The world the drive needs, set right after the shipped-defaults check and
 * before the generation: songSoaringOot on, through its own key with the
 * PRODUCTION mask (offered since #1169 XW12; no test seam, so the .redsave load
 * and the MM arrival take the world as a player's), and Shuffle Songs on
 * Anywhere (the drive's fixture: its pinned seed's placements were read under
 * it). Both keys are in memory only and go back at the verdict
 * (IntegrationTest_SoaringCleanup).
 */
static void SoarArmWorld(void) {
    IntegrationTest_SoaringSetCVarInt(Combo_ComboSettingKey(COMBO_SETTING_SONG_SOARING_OOT), 1);
    IntegrationTest_SoaringSetCVarInt(CVAR_RANDOMIZER_SETTING("ShuffleSongs"), RO_SONG_SHUFFLE_ANYWHERE);
    fprintf(stderr, "[PFC] soaring: songSoaringOot on (the production mask, no reveal), Shuffle Songs Anywhere\n");
    fflush(stderr);
}

// ============================================================================
// int-mm-warp-song-minuet (#1169 XW13): OoT's half
// ============================================================================
//
// The paired first crossing with songMinuetMm on. The first OoT leg is the
// paired one (Market, the chest, the Happy Mask Shop door). The MM leg is
// games/mm/2s2h/mm_int_warp_song_drive.cpp: it plays the Minuet in South Clock
// Town, answers the prompt OK, sees the one-shot target armed and hands the phase
// to OoT (GP_PHASE_OOT_RETURN) instead of firing the Clock Tower door. OoT's half
// here: the world (before the generation), the created file's record, the
// arrival at OoT scene init (the record's destination, derived, never a literal)
// and the verdict once Link stands idle there.

extern "C" EntranceInfo gEntranceTable[ENTR_MAX]; // games/oot/include/variables.h

namespace {
int16_t sMwArrivalScene = -1;
int32_t sMwArrivalEntrance = -1;
int sMwIdleFrames = 0;
bool sMwDone = false;
} // namespace

/** Where Minuet must land: the frozen record's destination (the pad when the
 *  record says unshuffled). */
static uint16_t MwExpectedArrival(void) {
    return Combo_MMWarpDrive_ExpectedArrival(0, Combo_OoTWarpRecord_Flags(), Combo_OoTWarpRecord_Dest(0));
}

/**
 * The world, set right after the shipped-defaults check and before the
 * generation: songMinuetMm on through its key, Shuffle Songs on Anywhere (under
 * Song Locations an MM extension joins the shared song pool, whose 23 checks it
 * could overflow; mm_warp_songs_mm_test.cpp does the same). Shuffle Warp Songs
 * stays at its default (off), so the record is PRESENT and unshuffled. The rule
 * is offered by the production mask (since #1169 XW13): no test reveal, so the
 * generation, the .redsave load and both arrivals take the world as a player's.
 */
static bool MwArmWorld(void) {
    IntegrationTest_SoaringSetCVarInt(Combo_ComboSettingKey(COMBO_SETTING_SONG_MINUET_MM), 1);
    IntegrationTest_SoaringSetCVarInt(CVAR_RANDOMIZER_SETTING("ShuffleSongs"), RO_SONG_SHUFFLE_ANYWHERE);
    // int-mm-warp-song-minuet-shared (#1169 XW14): the Shared Minuet of Forest on
    // too, through its key (put back at the verdict like the others).
    // The rule is offered by the production mask (since #1169 XW14): no test reveal.
    const bool shared = IntegrationTest_MMWarpSongMinuetShared();
    if (shared) {
        IntegrationTest_SoaringSetCVarInt(Combo_ComboSettingKey(COMBO_SETTING_SHARED_SONG_MINUET), 1);
    }
    // The rule must be offered before the seed is blamed for anything: a mask
    // that hides songMinuetMm again makes Generate refuse the word by name
    // (playthrough.cpp's hidden-rules gate), which the generation branch would
    // otherwise read as a bad pinned seed.
    const uint32_t word = Combo_ResolveComboItemRules2();
    char refused[160];
    const uint32_t refusedBits = Combo_ComboItemRules2Refused(word, refused, sizeof(refused));
    const bool offered = Combo_ComboItemRule2Offered(RSBS_XITEM2_SONG_MINUET_MM);
    const bool sharedOffered = Combo_ComboItemRule2Offered(RSBS_XITEM2_SHARED_SONG_MINUET);
    const bool sharedResolved = (word & RSBS_XITEM2_SHARED_SONG_MINUET) != 0u;
    if (!offered || refusedBits != 0u || (word & RSBS_XITEM2_SONG_MINUET_MM) == 0u ||
        (shared && (!sharedOffered || !sharedResolved)) || (!shared && sharedResolved)) {
        char reason[640];
        snprintf(reason, sizeof(reason),
                 "int-mm-warp-song-minuet%s, the world: this build does not offer songMinuetMm (offered %d) or "
                 "sharedSongMinuet (offered %d, resolved %d, wanted %d), or Generate would refuse the resolved second "
                 "word 0x%08X (refused 0x%08X: '%s'); the rule's mask or its key regressed, not the pinned seed",
                 shared ? "-shared" : "", offered ? 1 : 0, sharedOffered ? 1 : 0, sharedResolved ? 1 : 0,
                 shared ? 1 : 0, (unsigned)word, (unsigned)refusedBits, refused);
        IntegrationTest_GameplayFail(reason);
        return false;
    }
    fprintf(stderr,
            "[PFC] mm warp song: songMinuetMm on%s (the production mask, no reveal; resolved second word 0x%08X, "
            "none refused), Shuffle Songs Anywhere, Shuffle Warp Songs at its default (off)\n",
            shared ? " with sharedSongMinuet on" : "", (unsigned)word);
    fflush(stderr);
    return true;
}

/** The generation failed: say which of the three causes it was, so the log
 *  alone tells a refused rule, a song-capacity refusal and a bad pinned seed
 *  apart (the worker's refusal note, its stderr line, and the song pass's
 *  recorded refusal, warp_song_items.h). */
static void MwGenerationFailed(void) {
    const int refusalKind = Combo_GenRefusal_Take();
    char refusedLine[256] = "";
    const bool refusedLogged =
        IntegrationTest_StderrCaptureLast("[OoT] generation REFUSED", refusedLine, sizeof(refusedLine));
    const char* songSpots = Combo_SongSpotsLastRefusal();
    const char* noteLine = Combo_GenRefusal_Line(refusalKind);
    const char* cause = "no refusal was noted: int-mm-warp-song-minuet needs another pinned seed";
    if (refusalKind == RSBS_GEN_REFUSAL_HIDDEN_RULE) {
        cause = "the hidden-rules gate refused the world (a rule this build does not offer)";
    } else if (refusalKind == RSBS_GEN_REFUSAL_SONG_SPOTS) {
        cause = "a song-capacity refusal (the songs outnumber the song checks they can use)";
    } else if (refusedLogged || songSpots[0] != '\0') {
        cause = "a refusal with no note (read the refused line and the song-spots refusal)";
    }
    char reason[1024];
    snprintf(reason, sizeof(reason),
             "the pinned paired world did not generate with songMinuetMm on and Shuffle Songs Anywhere: %s "
             "[refusal note %d (%s), resolved second word 0x%08X, refused line '%s', song-spots refusal '%s']",
             cause, refusalKind, noteLine != NULL ? noteLine : "none", (unsigned)Combo_ResolveComboItemRules2(),
             refusedLine, songSpots);
    IntegrationTest_GameplayFail(reason);
}

// games/oot/soh/oot_shared_songs_test.cpp (#1169 XW10): how many of the generated
// world's OoT checks hold an RG_* id, or -1 with no seed context.
extern "C" int OoT_SharedSongsTest_WorldCopies(int rg);

/** The created file: the rule frozen on, the record PRESENT and unshuffled, and
 *  Minuet's destination its vanilla pad in a scene OoT's table names. Under the
 *  shared variant (#1169 XW14) also the shared Minuet frozen on and nothing owned
 *  yet, so the drive's later reads can only come from this run's play. */
static bool MwCheckCreatedWorld(char* msg, size_t cap) {
    const int status = Combo_OoTWarpRecord_Status();
    const uint8_t flags = Combo_OoTWarpRecord_Flags();
    const uint16_t dest = Combo_OoTWarpRecord_Dest(0);
    const uint16_t pad = Combo_MMWarpPlay_PadEntrance(0);
    const uint16_t expected = MwExpectedArrival();
    const char* why = NULL;
    if (!Combo_ComboCrossItemRule2(RSBS_XITEM2_SONG_MINUET_MM)) {
        why = "the creation did not freeze songMinuetMm on";
    } else if (status != COMBO_OOT_WARP_REC_PRESENT) {
        why = "the creation's warp-song record is not PRESENT";
    } else if ((flags & RSBS_OOT_WARP_REC_SHUFFLED) != 0u) {
        why = "the record says shuffled, but Shuffle Warp Songs was left at its default (off)";
    } else if (expected != dest || expected != pad) {
        why = "an unshuffled record's Minuet destination is not its pad";
    } else if (expected == 0u || expected >= ENTR_MAX) {
        why = "Minuet's destination is outside OoT's entrance table";
    } else if (IntegrationTest_MMWarpSongMinuetShared() && !Combo_ComboCrossItemRule2(RSBS_XITEM2_SHARED_SONG_MINUET)) {
        why = "the creation did not freeze sharedSongMinuet on";
    } else if (IntegrationTest_MMWarpSongMinuetShared() &&
               (Combo_SharedFamilies2Owned() != 0u || Combo_MMWarpSongsOwned() != 0u)) {
        why = "the created file already owns a shared warp song or an MM warp song (the drive could not tell the "
              "family's carry from a leftover)";
    }
    snprintf(msg, cap,
             "int-mm-warp-song-minuet%s, the created world: %s [rule2 0x%08X, record status %d flags 0x%02X Minuet "
             "dest 0x%04X, pad 0x%04X, expected 0x%04X, family owned2 0x%08X, MM ownership byte 0x%02X]",
             IntegrationTest_MMWarpSongMinuetShared() ? "-shared" : "", why != NULL ? why : "ok",
             (unsigned)Combo_ComboCrossItemFlags2(), status, (unsigned)flags, (unsigned)dest, (unsigned)pad,
             (unsigned)expected, (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_MMWarpSongsOwned());
    if (why != NULL) {
        return false;
    }
    if (IntegrationTest_MMWarpSongMinuetShared()) {
        // Diagnostic only: which game's check holds the pair's one copy (a pair row
        // places the hosting game's own member). The row proves the family's apply
        // whichever it is: an MM-side copy paid out at the start is MM's own give,
        // which never writes the apply's give record that PREPARE reads.
        fprintf(stderr,
                "[PFC] mm warp song (shared): OoT checks holding OoT's Minuet of Forest: %d (0 means the pair's copy "
                "is on a Majora's Mask check)\n",
                OoT_SharedSongsTest_WorldCopies((int)RG_MINUET_OF_FOREST));
    }
    fprintf(stderr,
            "[PFC] mm warp song: created with songMinuetMm frozen on and an unshuffled record; Minuet lands at "
            "0x%04X (scene %d)\n",
            (unsigned)expected, (int)gEntranceTable[expected].scene);
    fflush(stderr);
    return true;
}

/** int-mm-warp-song-minuet-shared (#1169 XW14): does OoT's live save hold its own
 *  Minuet of Forest, asked through OoT's registered member of the shared Minuet
 *  (the family's own question)? False when no member is registered. */
static bool MwOoTMinuetHeld(void) {
    ComboSharedFamilyMember m;
    return Combo_SharedFamily2MemberGet(GAME_OOT, RSBS_XITEM2_SHARED_SONG_MINUET, &m) && m.held != NULL &&
           m.held(m.nativeId);
}

/**
 * int-mm-warp-song-minuet-shared, the first OoT leg before the Happy Mask Shop
 * door: OoT is given its OWN Minuet of Forest through its registered family member
 * (never MM's copy), so the only way Majora's Mask can hold the song at PREPARE is
 * the family: OoT's harvest at the door's suspend, then MM's apply at its arrival.
 * MM's ownership byte must still be 0 after the give.
 */
static bool MwSharedGiveOoTMinuet(void) {
    ComboSharedFamilyMember m;
    char msg[512];
    if (!Combo_SharedFamily2MemberGet(GAME_OOT, RSBS_XITEM2_SHARED_SONG_MINUET, &m) || m.held == NULL ||
        m.give == NULL) {
        IntegrationTest_GameplayFail("int-mm-warp-song-minuet-shared, OoT's leg: OoT registers no member of the "
                                     "shared Minuet of Forest (GameExports' OoTSharedFamilyRegistrar)");
        return false;
    }
    const bool before = m.held(m.nativeId);
    if (!before) {
        m.give(m.nativeId); // the member's contract: give() only when held() is false
    }
    const bool after = m.held(m.nativeId);
    const uint32_t owned2 = Combo_SharedFamilies2Owned();
    const uint8_t mmOwned = Combo_MMWarpSongsOwned();
    if (!after || mmOwned != 0u) {
        snprintf(
            msg, sizeof(msg),
            "int-mm-warp-song-minuet-shared, OoT's leg: OoT's own Minuet (id %u) %s after its give, MM's ownership "
            "byte 0x%02X (want held and 0x00), family owned2 0x%08X, quest items 0x%08X",
            (unsigned)m.nativeId, after ? "held" : "NOT held", (unsigned)mmOwned, (unsigned)owned2,
            (unsigned)gSaveContext.inventory.questItems);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    fprintf(stderr,
            "[PFC] mm warp song (shared): OoT given its own Minuet of Forest before the door (id %u, held before %d, "
            "now 1); MM's ownership byte 0x00, family owned2 0x%08X (OoT's harvest at the door's suspend owns it)\n",
            (unsigned)m.nativeId, before ? 1 : 0, (unsigned)owned2);
    fflush(stderr);
    return true;
}

/** Every value the arrival is judged by, on one line. */
static void MwDescribeArrival(char* out, size_t cap, int16_t sceneNum, uint16_t entrance) {
    const uint16_t expected = MwExpectedArrival();
    ComboWarpTarget t;
    ComboWarp_Peek(&t);
    snprintf(out, cap,
             "scene %d, entrance 0x%04X, expected 0x%04X (scene %d), record status %d flags 0x%02X dest 0x%04X, pad "
             "0x%04X, linkAge %d, owned 0x%02X (family owned2 0x%08X, OoT's own Minuet %d), switch pending %d, switch "
             "requested %d; context: warp armed %d (source %d marker 0x%04X target %d 0x%04X kind %u), pending song "
             "%d, startup kind %d",
             (int)sceneNum, (unsigned)entrance, (unsigned)expected,
             expected < ENTR_MAX ? (int)gEntranceTable[expected].scene : -1, Combo_OoTWarpRecord_Status(),
             (unsigned)Combo_OoTWarpRecord_Flags(), (unsigned)Combo_OoTWarpRecord_Dest(0),
             (unsigned)Combo_MMWarpPlay_PadEntrance(0), (int)gSaveContext.linkAge, (unsigned)Combo_MMWarpSongsOwned(),
             (unsigned)Combo_SharedFamilies2Owned(), MwOoTMinuetHeld() ? 1 : 0, Combo_IsCrossGameSwitch() ? 1 : 0,
             Combo_IsGameSwitchRequested() ? 1 : 0, t.armed ? 1 : 0, (int)t.sourceGame, (unsigned)t.marker,
             (int)t.targetGame, (unsigned)t.targetEntrance, (unsigned)t.kind, Combo_MMWarpPlay_PendingSong(),
             Combo_GetStartupEntranceKindForGame("oot"));
}

/**
 * OoT scene init in GP_PHASE_OOT_RETURN: the warp-song arrival. Each check can
 * fail on its own regression: the entrance and its scene (the record ignored, or
 * MM's pad not resolved), the pending switch (a launcher that forgot
 * Entrance_ClearPendingSwitch), and the song still owned (a combo-context reinit
 * across the switch). The warp target and the pending latch are printed only:
 * every spend clears the target twice over, and every departure has already
 * taken the latch, so no single-line regression could make them fail here.
 */
static bool MwArrivalChecks(int16_t sceneNum, uint16_t entrance) {
    char state[768];
    MwDescribeArrival(state, sizeof(state), sceneNum, entrance);
    const uint16_t expected = MwExpectedArrival();
    const char* why = NULL;
    if (entrance != expected) {
        why = "OoT did not arrive at the record's destination";
    } else if (expected >= ENTR_MAX || sceneNum != gEntranceTable[expected].scene) {
        why = "OoT arrived at the destination's entrance in another scene";
    } else if (Combo_IsCrossGameSwitch()) {
        why = "the cross-game switch is still pending after the arrival";
    } else if ((Combo_MMWarpSongsOwned() & RSBS_MM_WARP_SONG_OWNED_MINUET) == 0u) {
        why = "Minuet of Forest (MM) is no longer owned after the crossing";
    } else if (IntegrationTest_MMWarpSongMinuetShared() && !MwOoTMinuetHeld()) {
        why = "OoT's own Minuet of Forest is no longer held after the round trip (the shared family took it away)";
    }
    if (why != NULL) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "int-mm-warp-song-minuet%s, the arrival: %s [%s]",
                 IntegrationTest_MMWarpSongMinuetShared() ? "-shared" : "", why, state);
        IntegrationTest_GameplayFail(msg);
        return false;
    }
    sMwArrivalScene = sceneNum;
    sMwArrivalEntrance = entrance;
    sMwIdleFrames = 0;
    fprintf(stderr, "[PFC] mm warp song 'the arrival': OoT landed at the record's destination [%s]\n", state);
    fflush(stderr);
    return true;
}

/**
 * Once the paired drive's frames of the return phase are done: Link must stand
 * idle, in the arrival's scene at its entrance, for 10 frames in a row, within
 * kMwVerdictBudget frames. Then the pass: the file is erased and the run ends.
 */
static void MwVerdict(PlayState* play, int frame) {
    static const int kMwVerdictBudget = 600;
    static const int kMwIdleNeeded = 10;
    if (sMwDone) {
        return;
    }
    char state[768];
    char msg[1024];
    const uint16_t entrance = (uint16_t)gSaveContext.entranceIndex;
    if (play->sceneNum != sMwArrivalScene || (int32_t)entrance != sMwArrivalEntrance) {
        sMwDone = true;
        MwDescribeArrival(state, sizeof(state), play->sceneNum, entrance);
        snprintf(msg, sizeof(msg),
                 "int-mm-warp-song-minuet, the verdict: Link left the arrival (scene %d, entrance 0x%04X at the "
                 "arrival) [%s]",
                 (int)sMwArrivalScene, (unsigned)sMwArrivalEntrance, state);
        IntegrationTest_GameplayFail(msg);
        return;
    }
    const bool idle = play->msgCtx.msgMode == MSGMODE_NONE && play->transitionTrigger == TRANS_TRIGGER_OFF &&
                      play->transitionMode == TRANS_MODE_OFF && play->csCtx.state == CS_STATE_IDLE &&
                      gSaveContext.gameMode == GAMEMODE_NORMAL;
    sMwIdleFrames = idle ? sMwIdleFrames + 1 : 0;
    if (sMwIdleFrames < kMwIdleNeeded) {
        if (frame >= kMwVerdictBudget) {
            sMwDone = true;
            MwDescribeArrival(state, sizeof(state), play->sceneNum, entrance);
            snprintf(msg, sizeof(msg),
                     "int-mm-warp-song-minuet, the verdict: Link never stood idle for %d frames within %d frames of "
                     "the return phase's window [msgMode %d, transition %d/%d, cutscene state %d, gameMode %d; %s]",
                     kMwIdleNeeded, kMwVerdictBudget, (int)play->msgCtx.msgMode, (int)play->transitionTrigger,
                     (int)play->transitionMode, (int)play->csCtx.state, (int)gSaveContext.gameMode, state);
            IntegrationTest_GameplayFail(msg);
        }
        return;
    }
    sMwDone = true;
    fprintf(stderr,
            "[PFC] PASS: int-mm-warp-song-minuet: Minuet of Forest played in Majora's Mask crossed to Ocarina of "
            "Time as a child at the record's destination (scene %d, entrance 0x%04X), restored, the switch spent\n",
            (int)play->sceneNum, (unsigned)entrance);
    fflush(stderr);
    IntegrationTest_SetGameplayPhase(GP_PHASE_DONE);
    // A pass leaves no file behind (a failure keeps it for forensics; the next
    // paired run erases it by its name).
    Save_DeleteFile(kPfcSlot);
    fprintf(stderr, "[PFC] PASS: the Minuet of Forest crossing; erased file %d\n", kPfcSlot + 1);
    fflush(stderr);
    IntegrationTest_SignalBootComplete(GAME_OOT, "Minuet of Forest crossing from Majora's Mask complete");
}

// ============================================================================
// Integration Test Hooks
// ============================================================================

// OoT's line in the wall-clock watchdog's dump (#793). Runs on the watchdog
// thread while OoT's own thread is wedged: plain reads, no dereference of the
// play state (it may be mid-teardown), no FILE lock.
static void OoT_IntegrationWatchdogDescribe(char* out, size_t cap) {
    snprintf(out, cap, "gameMode=%d entranceIndex=0x%04X cutsceneIndex=0x%04X gGameState=%p gPlayState=%p%s%s",
             (int)gSaveContext.gameMode, (uint16_t)gSaveContext.entranceIndex, (uint16_t)gSaveContext.cutsceneIndex,
             (void*)OoT_gGameState, (void*)OoT_gPlayState,
             IntegrationTest_GetMode() == INT_TEST_SWITCH_OOT_HMS_TO_MM ? " T1 stage=" : "",
             IntegrationTest_GetMode() == INT_TEST_SWITCH_OOT_HMS_TO_MM ? T1StageName(sT1Stage) : "");
}

/**
 * Register integration test hooks for OoT.
 * Called after OoT is initialized when integration test mode is active.
 */
static void OoT_RegisterIntegrationTestHooks(void) {
    if (!IntegrationTest_IsActive()) {
        return;
    }

    IntegrationTestMode mode = IntegrationTest_GetMode();

    // Every mode: each OoT frame bumps the int-* rows' progress word, which the
    // wall-clock watchdog thread reads (#793). Registered once, from OoT's first
    // init; resumed OoT frames fire it too.
    IntegrationTest_WatchdogSetDescriber(GAME_OOT, OoT_IntegrationWatchdogDescribe);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
        if (Context_GetCurrentGame() == GAME_OOT) {
            IntegrationTest_FrameProgress(GAME_OOT);
        }
    });

    if (mode == INT_TEST_BOOT_OOT) {
        fprintf(stderr, "[OoT] Registering integration test hooks for boot detection\n");
        fflush(stderr);

        // Register hook for title screen init
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleInit>([](void* gameState) {
            fprintf(stderr, "[OoT-INT-TEST] OnZTitleInit hook fired!\n");
            fflush(stderr);
            IntegrationTest_SignalBootComplete(GAME_OOT, "title screen init");
        });

        // Register hook for file select presentation
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>([]() {
            fprintf(stderr, "[OoT-INT-TEST] OnPresentFileSelect hook fired!\n");
            fflush(stderr);
            IntegrationTest_SignalBootComplete(GAME_OOT, "file select presented");
        });

        fprintf(stderr, "[OoT] Integration test hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_SWITCH_OOT_HMS_TO_MM || mode == INT_TEST_RESOURCE_CACHE_OWNER) {
        // T1 (#260, #544): reach live OoT gameplay without a Start press, fire
        // the Happy Mask Shop entrance from there, assert the cross-game switch
        // resolves to MM South Clock Town (the tower-exit arrival). Leg 1 passes
        // when routing is verified here; the final pass is signaled from the
        // MM-side hook after MM stabilizes post-switch. Stage machine and
        // watchdog: T1Stage above.
        fprintf(stderr, "[OoT] Registering integration test hooks for HMS->MM switch (T1)\n");
        fflush(stderr);

        sT1Stage = T1_STAGE_BOOT;
        sT1GameplayFrames = 0;
        sT1StageStart = std::chrono::steady_clock::now();

        // Boot injection, whichever fires first (both are no-ops past
        // T1_STAGE_BOOT). The title hook ticks every frame of the boot logo, so
        // an unattended run always reaches it; file select is belt-and-braces
        // for a boot sequence that skips straight there.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleUpdate>(
            [](void* gameState) { T1InjectFrom((GameState*)gameState, "title screen"); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>(
            []() { T1InjectFrom(OoT_gGameState, "file select"); });

        // Gameplay reached: OoT_Play_Init built the scene the debug save asked
        // for. Fires after the startup-entrance consumption, so entranceIndex
        // is final here.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
            if (Context_GetCurrentGame() != GAME_OOT || sT1Stage != T1_STAGE_ENTERING_PLAY) {
                return; // shared hook storage guard (#344); only the injected arrival counts
            }
            uint16_t entrance = (uint16_t)gSaveContext.entranceIndex;
            fprintf(stderr, "[OoT-INT-TEST] OoT scene init: scene %d, entrance 0x%04X, gameMode %d\n", sceneNum,
                    entrance, (int)gSaveContext.gameMode);
            fflush(stderr);
            if (entrance != kT1BootEntrance) {
                char msg[112];
                snprintf(msg, sizeof(msg), "gameplay arrived at entrance 0x%04X, expected 0x%04X", entrance,
                         kT1BootEntrance);
                T1Fail(msg);
                return;
            }
            sT1GameplayFrames = 0;
            T1SetStage(T1_STAGE_GAMEPLAY);
        });

        // Frame driver + wall-clock budget check. Runs on every OoT gamestate's
        // frame (title, opening, file select, play), so a stage before the
        // trigger that stalls while frames keep ticking is caught here. A wedge
        // inside a frame never returns to this hook (see the T1 block comment).
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            if (Context_GetCurrentGame() != GAME_OOT) {
                return; // (#344) MM frames share this hook storage
            }
            if (sT1Stage == T1_STAGE_TRIGGERED || sT1Stage == T1_STAGE_FAILED) {
                return;
            }
            if (T1SecondsInStage() >= kT1StageBudgetSecs) {
                switch (sT1Stage) {
                    case T1_STAGE_BOOT:
                        T1Fail("title screen / file select never presented: the debug save was never injected");
                        break;
                    case T1_STAGE_ENTERING_PLAY:
                        T1Fail("gameplay never reached: no OoT scene init after the debug-save injection");
                        break;
                    default:
                        T1Fail("gameplay reached but never ran live frames (no PlayState / not GAMEMODE_NORMAL)");
                        break;
                }
                return;
            }
            if (sT1Stage != T1_STAGE_GAMEPLAY) {
                return;
            }
            PlayState* play = OoT_gPlayState;
            if (play == NULL || OoT_gGameState != &play->state || gSaveContext.gameMode != GAMEMODE_NORMAL ||
                GET_PLAYER(play) == NULL) {
                sT1GameplayFrames = 0;
                return;
            }
            if (++sT1GameplayFrames < kT1GameplayFramesBeforeTrigger) {
                return;
            }
            T1TriggerHmsAndAssertRouting();
        });

        // #981: int-resource-cache-owner rides T1 to MM (its leg 1 is in
        // T1TriggerHmsAndAssertRouting), and MM walks back through the Clock Tower
        // door once its leg passes. Back in OoT, 10 frames in, OoT must be served
        // the objects it had before it left.
        if (mode == INT_TEST_RESOURCE_CACHE_OWNER) {
            sRcoReturnFrames = 0;
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
                if (Context_GetCurrentGame() != GAME_OOT || !ResourceCacheOwner_MMLegPassed()) {
                    sRcoReturnFrames = 0;
                    return;
                }
                if (++sRcoReturnFrames != 10) {
                    return;
                }
                char msg[384];
                if (!RcoOoTCheck("back from MM", msg, sizeof(msg)) ||
                    !ResourceCacheOwner_PatchProbeOoTReturn(msg, sizeof(msg))) {
                    fprintf(stderr, "[OoT-INT-TEST] FAIL (int-resource-cache-owner): %s\n", msg);
                    fflush(stderr);
                    IntegrationTest_RequestExit();
                    Combo_RequestGameSwitch();
                    return;
                }
                fprintf(stderr, "[OoT-INT-TEST] PASS cache-owner leg 3: %s\n", msg);
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_OOT, "each game served its own copies; OoT's survived");
            });
        }

        fprintf(stderr, "[OoT] HMS->MM switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_CROSSING_REFUSED) {
        CrRegisterHooks();
    } else if (mode == INT_TEST_SWITCH_MM_CLOCKTOWN_SOUTH_TO_OOT) {
        // T2 (#261) leg 2: OoT has been booted via the cross-game switch from
        // MM's SCT-south trigger. Reaching this hook means MM's freeze + main
        // loop's hand-off + OoT_Game_Init all succeeded end-to-end. Signal pass
        // once OoT's graph thread is running steady frames.
        fprintf(stderr, "[OoT] Registering integration test hooks for SCT-south->OoT switch completion (T2)\n");
        fflush(stderr);

        sOoTGameStateMainFrameCount = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            sOoTGameStateMainFrameCount++;
            if (sOoTGameStateMainFrameCount >= 10) {
                fprintf(stderr, "[OoT-INT-TEST] OoT stable after SCT-south->OoT switch (frame %d)\n",
                        sOoTGameStateMainFrameCount);
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_OOT, "OoT stable after SCT-south->OoT switch");
            }
        });

        fprintf(stderr, "[OoT] SCT-south->OoT switch hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_ARCHIVE_HOTSWAP_CYCLE) {
        // T4 (#263): drive >=3 OoT<->MM archive hot-swaps and assert a healthy
        // runtime. OoT boots first, so OoT is arrivals #1 and #3 of the
        // OoT->MM->OoT->MM cycle (4 arrivals == 3 transitions).
        //
        // Registration runs from OoT_Game_Init, which fires only on OoT's FIRST
        // entry — later OoT arrivals come back through OoT_Game_Resume (see
        // GameRunner_SwitchTo: a suspended game is resumed, not re-init'd), so
        // this hook is registered exactly once and the persistent frame counter
        // is NOT reset per arrival. The hook therefore re-arms itself: it fires
        // ~10 stable frames after each (re)entry, then resets the counter so the
        // next arrival reached via resume is detected the same way.
        fprintf(stderr, "[OoT] Registering integration test hooks for archive-hotswap cycle (T4)\n");
        fflush(stderr);

        sOoTGameStateMainFrameCount = 0;

        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            // Fire once per arrival, ~10 stable frames after (re)entry, then
            // re-arm for the next OoT arrival (reached via OoT_Game_Resume).
            // (#344) Both games' frame loops fire this shared hook storage;
            // only count OoT's own frames so MM frames can't record a
            // bogus OoT arrival.
            if (Context_GetCurrentGame() != GAME_OOT) {
                sOoTGameStateMainFrameCount = 0;
                return;
            }
            sOoTGameStateMainFrameCount++;
            if (sOoTGameStateMainFrameCount < 10) {
                return;
            }
            sOoTGameStateMainFrameCount = 0;

            int n = ArchiveHotswap_RecordArrival();
            fprintf(stderr, "[OoT-INT-TEST] OoT stable; archive-hotswap arrival #%d of %d\n", n,
                    ArchiveHotswap_TargetArrivals());
            fflush(stderr);

            // #981: OoT is served its own copies of the paths MM also carries.
            SkeletonHeader* skel = ResourceMgr_LoadSkeletonByName(kRcoOctorokSkel, NULL);
            char rco[512];
            if (!ResourceCacheOwner_CheckArrival(GAME_OOT, n, skel, skel != NULL ? skel->limbCount : -1, rco,
                                                 sizeof(rco))) {
                fprintf(stderr, "[OoT-INT-TEST] FAIL: resource cache: %s\n", rco);
                fflush(stderr);
                IntegrationTest_RequestExit();
                Combo_RequestGameSwitch();
                return;
            }
            fprintf(stderr, "[OoT-INT-TEST] resource cache: %s\n", rco);

            if (ArchiveHotswap_RssExceeded()) {
                // Steady-state RSS blew the bound — the #154 per-switch leak
                // regression. Fail fast: RequestExit does NOT set the pass
                // flag, so the run returns non-zero. Combo_RequestGameSwitch()
                // right after unblocks the main loop promptly (known fix).
                fprintf(stderr, "[OoT-INT-TEST] FAIL: steady-state RSS bound exceeded after %d arrivals\n", n);
                fflush(stderr);
                IntegrationTest_RequestExit();
                Combo_RequestGameSwitch();
            } else if (n >= ArchiveHotswap_TargetArrivals()) {
                // Target arrivals reached with a healthy runtime — PASS.
                // SignalBootComplete sets the pass flag and requests the
                // switch that unblocks the main loop.
                fprintf(stderr, "[OoT-INT-TEST] archive-hotswap cycle complete after %d arrivals\n", n);
                fflush(stderr);
                IntegrationTest_SignalBootComplete(GAME_OOT, "archive-hotswap cycle complete");
            } else {
                // Keep the cycle going: re-trigger the OoT->MM switch via the
                // Happy Mask Shop entrance — same call the T1 branch makes.
                fprintf(stderr, "[OoT-INT-TEST] re-triggering HMS entrance 0x%04X to continue cycle\n",
                        OOT_ENTR_HAPPY_MASK_SHOP);
                fflush(stderr);
                Combo_CheckCrossGameEntrance("oot", OOT_ENTR_HAPPY_MASK_SHOP);
            }
        });

        fprintf(stderr, "[OoT] archive-hotswap cycle hooks registered\n");
        fflush(stderr);
    } else if (mode == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        // Full operator-repro loop: debug save -> live gameplay -> Happy Mask
        // Shop door (production Combo_CheckEntranceSwitch path, WITH the
        // SaveContext freeze) -> MM Clock Tower -> SCT-south exit -> OoT
        // RESUME leg (the crash surface of the 2026-07 logs) -> debug warp ->
        // final door transition. The MM half lives in
        // games/mm/2s2h/GameExports_SingleExe.cpp; the phase machine is shared
        // via integration_test_hooks.h.
        fprintf(stderr, "[OoT] Registering gameplay round-trip hooks\n");
        fflush(stderr);

        sGpArrivalPhase = GP_PHASE_DONE;
        sGpPlayerLastPhase = GP_PHASE_DONE;
        sGpFramesInPhase = 0;
        sGpSceneInits = 0;
        // Wall-clock budget per OoT-owned phase, sized well under the CTest
        // TIMEOUT so the watchdog's diagnostic dump fires BEFORE the hard kill
        // (#376 item 4). The stall timer is re-based below whenever the phase
        // advances or completes a gameplay frame (#882), so only a genuinely
        // wedged phase reaches the budget.
        IntegrationTest_GameplayWatchdogRebase(&sGpWatchdogTimer, (int)GP_PHASE_DONE, 0, GpWatchdogNowSecs());
        sGpWatchdogBudgetSecs = IntegrationTest_GetGameplayConfig()->watchdogSecs;
        sGpWatchdogFired = false;

        // Boot injection — whichever of these fires first wins; both are
        // no-ops once the phase machine has left GP_PHASE_BOOT. The title
        // hook receives its gamestate (TitleContext, whose first member is
        // the GameState); the file-select hook has no argument, so it uses
        // the OoT_gGameState global (same pattern as debugconsole.cpp).
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleUpdate>([](void* gameState) {
            if (IntegrationTest_GetGameplayPhase() != GP_PHASE_BOOT) {
                return;
            }
            GpBootInject((GameState*)gameState, "title screen", false);
        });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPresentFileSelect>([]() {
            if (IntegrationTest_GetGameplayPhase() != GP_PHASE_BOOT) {
                return;
            }
            GpBootInject(OoT_gGameState, "file select", true);
        });
        // The paired variant's creation runs from the file select's own main
        // (every frame, from the first one), not from OnPresentFileSelect, which
        // waits for a Start press no unattended run sends (#544).
        if (IntegrationTest_PairedFirstCrossing()) {
            // The "no refusal toast" checks read the test-only toast record,
            // which is off in every other process.
            OoT_Notification_RecordForTest(1);
        }
        if (IntegrationTest_PairedFirstCrossing() && !IntegrationTest_PairedSkipCreation()) {
            sPfcTitleRedirected = false;
            sPfcCreationAttempted = false;
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnFileChooseMain>([](void* gameState) {
                if (IntegrationTest_GetGameplayPhase() != GP_PHASE_BOOT) {
                    return;
                }
                GpBootInject((GameState*)gameState, "file select", true);
            });
        }
        if (IntegrationTest_SoaringToOwl()) {
            // #1169 XW6: the Soar verdict and the fade's watch (Link is frozen
            // from the departure on, so the frame start is where they run).
            sSoar = SoarState{};
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() { SoarMainStart(); });
            GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() { SoarPlayDestroy(); });
        }

        // Arrival tracking + entrance verification. Fires from the scene
        // build inside OoT_Play_Init — i.e. AFTER the startup-entrance
        // consumption at the top of Play_Init, so entranceIndex is final.
        // The return-leg check is the #356 regression predicate: an MM
        // entrance id (0xC010) surviving into OoT would land here as a
        // mismatch (or crash first, which the CI wrapper reports with logs).
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t sceneNum) {
            if (Context_GetCurrentGame() == GAME_MM) {
                return; // shared hook storage guard (#344)
            }
            sGpSceneInits++;
            GameplayPhase phase = IntegrationTest_GetGameplayPhase();
            const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
            uint16_t entrance = (uint16_t)gSaveContext.entranceIndex;
            // int-soaring-to-owl (#1169 XW6): the Windmill arrival is the drive's,
            // and an OoT scene after Soar fails the run.
            if (IntegrationTest_SoaringToOwl() && SoarSceneInit(phase, sceneNum, entrance)) {
                return;
            }
            uint16_t expected;
            const char* what;
            switch (phase) {
                case GP_PHASE_OOT_PRE:
                    expected = cfg->bootEntrance;
                    what = "boot";
                    break;
                case GP_PHASE_OOT_RETURN:
                    // int-mm-warp-song-minuet (#1169 XW13): the warp-song arrival,
                    // at the record's destination instead of the Market.
                    expected =
                        IntegrationTest_MMWarpSongMinuet() ? MwExpectedArrival() : OOT_ENTR_MARKET_FROM_MASK_SHOP;
                    what = IntegrationTest_MMWarpSongMinuet() ? "warp-song arrival" : "return leg";
                    break;
                case GP_PHASE_OOT_WARP:
                    expected = cfg->warpEntrance;
                    what = "warp";
                    break;
                case GP_PHASE_OOT_EXIT:
                    expected = cfg->exitEntrance;
                    what = "exit door";
                    break;
                default:
                    return; // MM-owned or completed phase: not an arrival we track
            }
            fprintf(stderr, "[GP-TEST] OoT scene init #%d: scene %d, entrance 0x%04X (%s)\n", sGpSceneInits, sceneNum,
                    entrance, what);
            fflush(stderr);
            if (phase == GP_PHASE_OOT_RETURN && IntegrationTest_MMWarpSongMinuet() &&
                !MwArrivalChecks(sceneNum, entrance)) {
                return;
            }
            if (entrance != expected) {
                char msg[160];
                snprintf(msg, sizeof(msg), "%s arrived at entrance 0x%04X, expected 0x%04X — entrance corruption?",
                         what, entrance, expected);
                IntegrationTest_GameplayFail(msg);
                return;
            }
            // Hard-assert the forced-child-on-return contract (operator
            // decision: the MM trip is child-canon; OoT_Game_Resume
            // forces linkAge child after restoring the frozen save). A
            // regression passes the entrance check above but lands here
            // as LINK_AGE_ADULT.
            if (phase == GP_PHASE_OOT_RETURN && gSaveContext.linkAge != LINK_AGE_CHILD) {
                char ageMsg[128];
                snprintf(ageMsg, sizeof(ageMsg),
                         "return leg arrived as linkAge=%d, expected LINK_AGE_CHILD (%d) — "
                         "force-child-on-return regressed",
                         (int)gSaveContext.linkAge, (int)LINK_AGE_CHILD);
                IntegrationTest_GameplayFail(ageMsg);
                return;
            }
            // Frame-counter reset lock. The scene build runs after the
            // frame-counter block in OoT_Play_Init, so an arrival must land
            // here with a counter that was just zeroed. Left stale, the
            // suspended session's value (or MM's arena leftovers) rides into
            // the arrival and voids the interval autosave's `gameplayFrames <
            // 60` warm-up guard (soh/Enhancements/QoL/Autosave.cpp) — a save
            // can then fire during the arrival fade. Return leg only: the warp
            // and exit phases are ordinary in-game door transitions, where a
            // large counter is the correct session-long value.
            if (phase == GP_PHASE_OOT_RETURN && OoT_gPlayState != NULL) {
                fprintf(stderr, "[GP-TEST] return leg gameplayFrames=%u (expect 0)\n",
                        (unsigned)OoT_gPlayState->gameplayFrames);
                fflush(stderr);
                if (OoT_gPlayState->gameplayFrames != 0) {
                    char frameMsg[176];
                    snprintf(frameMsg, sizeof(frameMsg),
                             "return leg arrived with gameplayFrames=%u, expected 0 — the arrival frame-counter "
                             "reset regressed, so the autosave warm-up guard is void on re-arrivals",
                             (unsigned)OoT_gPlayState->gameplayFrames);
                    IntegrationTest_GameplayFail(frameMsg);
                    return;
                }
            }
            // Demo-state leakage asserts (the bug 1a/1b/1c common-cause
            // class): every cross-game arrival and post-return load must be a
            // plain gameplay spawn. A title/attract gameMode, a live cutscene
            // index, a cutscene scene layer, or a queued nextCutsceneIndex
            // here means frozen-blob or title-chain state escaped the
            // consumption-point neutralization.
            if (phase == GP_PHASE_OOT_RETURN || phase == GP_PHASE_OOT_WARP) {
                // The camera constants must be seeded on every arrival. Main()
                // re-mints gGameInfo (zeroing the REG backing store) on each
                // OoT entry, while the seeding sat behind a once-per-process
                // latch — so every return leg used to run the camera on
                // all-zero constants: it translated with Link but never
                // reoriented to follow him, and Player's stick-to-world yaw
                // (derived from the camera) went with it.
                if (!OoT_Camera_RegsSeeded()) {
                    IntegrationTest_GameplayFail("camera constant registers are zeroed on arrival — the "
                                                 "once-only OREG seeding lost its resume inverse");
                    return;
                }
                if (gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.cutsceneIndex >= 0xFFF0 ||
                    gSaveContext.sceneSetupIndex >= 4 || gSaveContext.nextCutsceneIndex != 0xFFEF) {
                    char stateMsg[192];
                    snprintf(stateMsg, sizeof(stateMsg),
                             "%s arrived with demo state: gameMode=%d cutsceneIndex=0x%04X sceneSetupIndex=%d "
                             "nextCutsceneIndex=0x%04X — title/cutscene state leaked into a gameplay spawn",
                             what, (int)gSaveContext.gameMode, (uint16_t)gSaveContext.cutsceneIndex,
                             (int)gSaveContext.sceneSetupIndex, (uint16_t)gSaveContext.nextCutsceneIndex);
                    IntegrationTest_GameplayFail(stateMsg);
                    return;
                }
            }
            if (phase == GP_PHASE_OOT_RETURN && IntegrationTest_PairedFirstCrossing() && !GpPairedCheckReturn()) {
                return;
            }
            sGpArrivalPhase = phase;
        });

        // Per-frame gameplay driver: counts live-gameplay frames (player
        // actor updating in the arrived scene) and fires the next action
        // when the window completes.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
            if (Context_GetCurrentGame() == GAME_MM) {
                return; // shared hook storage guard (#344)
            }
            if (OoT_gPlayState == NULL) {
                return;
            }
            GameplayPhase phase = IntegrationTest_GetGameplayPhase();
            if (phase != sGpPlayerLastPhase) {
                sGpPlayerLastPhase = phase;
                sGpFramesInPhase = 0;
                sGpCamProbeArmed = 0;
            }
            if (sGpArrivalPhase != phase) {
                return; // fade-out after firing a door, or not yet in the expected scene
            }
            const GameplayTestConfig* cfg = IntegrationTest_GetGameplayConfig();
            PlayState* play = OoT_gPlayState;
            sGpFramesInPhase++;
            if (phase == GP_PHASE_OOT_PRE && IntegrationTest_PairedFirstCrossing()) {
                GpPairedTrackerPlaytest(sGpFramesInPhase);
                // #1542 S3 (RSBS_PFC_SONG_HOST=oot): an MM item on an OoT song check, live.
                if (GpSongHostOoT()) {
                    GpSongHostOoTFrame(play, sGpFramesInPhase);
                }
            }
            if (sGpFramesInPhase == 20 && (phase == GP_PHASE_OOT_PRE || phase == GP_PHASE_OOT_RETURN) &&
                IntegrationTest_PairedFirstCrossing() && !IntegrationTest_PairedSkipCreation()) {
                GpPairedCheckStoneHints(phase == GP_PHASE_OOT_PRE ? "first OoT leg" : "return leg");
            }
            if (phase == GP_PHASE_OOT_WARP && IntegrationTest_PairedFirstCrossing()) {
                PfcShopPlaytestWarpFrame(play, sGpFramesInPhase);
                PfcForeignModelPlaytestFrame(play, sGpFramesInPhase, cfg->warpFrames);
            }

            // Door-actor presence (bug 1a): by frame 30 the arrival scene's
            // transition actors have spawned. Baseline in the boot phase;
            // compare on the return leg when the scene matches (default route:
            // both are Market via 0x01D1). Fewer doors than first boot means
            // the switch poisoned the cached transition-actor list.
            if (sGpFramesInPhase == 30) {
                int doorCount = play->actorCtx.actorLists[ACTORCAT_DOOR].length;
                if (phase == GP_PHASE_OOT_PRE) {
                    sGpDoorBaseline = doorCount;
                    sGpDoorBaselineScene = play->sceneNum;
                    fprintf(stderr, "[GP-TEST] door baseline: %d door actors in scene %d\n", doorCount,
                            (int)play->sceneNum);
                    fflush(stderr);
                } else if (phase == GP_PHASE_OOT_RETURN && sGpDoorBaseline >= 0 &&
                           play->sceneNum == sGpDoorBaselineScene) {
                    fprintf(stderr, "[GP-TEST] door check: %d door actors in scene %d (baseline %d)\n", doorCount,
                            (int)play->sceneNum, sGpDoorBaseline);
                    fflush(stderr);
                    if (doorCount < sGpDoorBaseline) {
                        char doorMsg[160];
                        snprintf(doorMsg, sizeof(doorMsg),
                                 "return leg scene %d has %d door actors, first boot had %d — transition "
                                 "actors lost across the switch (bug 1a)",
                                 (int)play->sceneNum, doorCount, sGpDoorBaseline);
                        IntegrationTest_GameplayFail(doorMsg);
                        return;
                    }
                }
            }

            // Camera-follow assert (bug 1b), WARP phase only: frames 20..39
            // arm a snapshot of the active camera + player once the frame is
            // settled plain gameplay; the driver then force-marches the player
            // (+4/frame — the harness has no input injection, so Link never
            // moves on his own); frame 80 requires the camera to have tracked.
            // WARP-only because a march in the return-leg Market crosses a
            // scene-exit trigger and aborts the phase (found the hard way);
            // the operator's camera bug lives in warp-class arrivals (Hyrule
            // Field 0x00CD). If geometry walls the march in, playerDist stays
            // small and the assert self-vacuouses (lost coverage, never a
            // false positive). Disable with RSBS_GP_CAMERA_ASSERT=0.
            if (cfg->cameraAssert && cfg->framesPerPhase >= 90 && phase == GP_PHASE_OOT_WARP) {
                Actor* playerActor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
                Camera* cam = play->cameraPtrs[play->activeCamera];
                if (playerActor != NULL && cam != NULL) {
                    // Arm anywhere in frames 20..39 (transitions can settle a
                    // few frames late); log the gate state if it never opens —
                    // silent non-coverage is how vacuous asserts are born.
                    if (!sGpCamProbeArmed && sGpFramesInPhase >= 20 && sGpFramesInPhase < 40 &&
                        gSaveContext.gameMode == GAMEMODE_NORMAL && play->csCtx.state == CS_STATE_IDLE &&
                        play->transitionMode == TRANS_MODE_OFF) {
                        sGpCamStartEye = cam->eye;
                        sGpCamStartAt = cam->at;
                        sGpCamStartPlayer = playerActor->world.pos;
                        sGpCamProbeArmed = 1;
                    }
                    if (!sGpCamProbeArmed && sGpFramesInPhase == 40) {
                        fprintf(stderr,
                                "[GP-TEST] WARNING: camera probe never armed (gameMode=%d csState=%d "
                                "transitionMode=%d) — camera-follow assert skipped this phase\n",
                                (int)gSaveContext.gameMode, (int)play->csCtx.state, (int)play->transitionMode);
                        fflush(stderr);
                    }
                    if (sGpCamProbeArmed && sGpFramesInPhase < 80) {
                        playerActor->world.pos.x += 4.0f;
                    }
                    if (sGpCamProbeArmed && sGpFramesInPhase == 80) {
                        float playerDist = GpVecDist(&playerActor->world.pos, &sGpCamStartPlayer);
                        // eye and at MUST be judged separately. A camera
                        // bolted in place that merely rotates to keep Link in
                        // frame — the degenerate state Camera_Update falls
                        // into when it skips the setting/mode engine — moves
                        // `at` by roughly the player's displacement while
                        // `eye` never moves at all. Summing the two hides
                        // exactly the failure this assert exists to catch.
                        float eyeDist = GpVecDist(&cam->eye, &sGpCamStartEye);
                        float atDist = GpVecDist(&cam->at, &sGpCamStartAt);
                        fprintf(stderr,
                                "[GP-TEST] camera-follow: player moved %.1f, camera eye moved %.1f, at moved %.1f "
                                "(status=%d setting=%d mode=%d)\n",
                                playerDist, eyeDist, atDist, (int)cam->status, (int)cam->setting, (int)cam->mode);
                        fflush(stderr);
                        sGpCamProbeArmed = 0;
                        if (playerDist > 80.0f && eyeDist < 5.0f) {
                            char camMsg[224];
                            snprintf(camMsg, sizeof(camMsg),
                                     "camera did not follow: player moved %.1f but camera eye moved %.1f "
                                     "(at moved %.1f, status=%d setting=%d mode=%d) — camera is anchored",
                                     playerDist, eyeDist, atDist, (int)cam->status, (int)cam->setting, (int)cam->mode);
                            IntegrationTest_GameplayFail(camMsg);
                            return;
                        }
                    }
                }
            }

            // int-soaring-to-owl (#1169 XW6): once the paired drive's own frames
            // of the phase are done, the Soaring legs own it. They fire the
            // Happy Mask Shop door after the first leg's refusals, and the return
            // leg ends in the Soaring departure instead of the warp.
            if (IntegrationTest_SoaringToOwl() && (phase == GP_PHASE_OOT_PRE || phase == GP_PHASE_OOT_RETURN)) {
                if (sGpFramesInPhase >= cfg->framesPerPhase) {
                    SoarFrame(play, phase);
                }
                return;
            }
            // int-mm-warp-song-minuet (#1169 XW13): the warp-song arrival ends the
            // run (no record cycle, no debug warp, no exit door).
            if (IntegrationTest_MMWarpSongMinuet() && phase == GP_PHASE_OOT_RETURN) {
                if (sGpFramesInPhase >= cfg->framesPerPhase) {
                    MwVerdict(play, sGpFramesInPhase - cfg->framesPerPhase);
                }
                return;
            }

            // The warp phase gets its own (usually longer) budget so time-
            // dependent faults inside the warp target can soak.
            {
                int phaseBudget = (phase == GP_PHASE_OOT_WARP) ? cfg->warpFrames : cfg->framesPerPhase;
                if (sGpFramesInPhase < phaseBudget) {
                    return;
                }
            }
            switch (phase) {
                case GP_PHASE_OOT_PRE:
                    // int-mm-warp-song-minuet-shared (#1169 XW14): OoT learns its
                    // own Minuet before the door; MM must then hold it by the family.
                    if (IntegrationTest_MMWarpSongMinuetShared() && !MwSharedGiveOoTMinuet()) {
                        return;
                    }
                    if (IntegrationTest_PairedFirstCrossing()) {
                        // Frozen with the live save by the door's cross-game switch.
                        gSaveContext.deaths = kPfcDeathsSentinel;
                        fprintf(stderr, "[PFC] OoT-half sentinel armed before the door: deaths=%u\n",
                                (unsigned)kPfcDeathsSentinel);
                        IntegrationTest_PairedMarkCrossing("Majora's Mask");
                    }
                    GpFireOoTDoor(OOT_ENTR_HAPPY_MASK_SHOP, "Happy Mask Shop door");
                    IntegrationTest_SetGameplayPhase(GP_PHASE_MM_STABILIZE);
                    break;
                case GP_PHASE_OOT_RETURN:
                    IntegrationTest_GameplayRecordCycle();
                    if (IntegrationTest_GameplayCyclesDone() < cfg->cycles) {
                        if (IntegrationTest_PairedFirstCrossing()) {
                            IntegrationTest_PairedMarkCrossing("Majora's Mask");
                        }
                        GpFireOoTDoor(OOT_ENTR_HAPPY_MASK_SHOP, "Happy Mask Shop door (next round trip)");
                        IntegrationTest_SetGameplayPhase(GP_PHASE_MM_STABILIZE);
                    } else {
                        GpFireOoTDoor(cfg->warpEntrance, "post-return debug warp");
                        IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_WARP);
                    }
                    break;
                case GP_PHASE_OOT_WARP:
                    GpFireOoTDoor(cfg->exitEntrance, "final door transition");
                    IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_EXIT);
                    break;
                case GP_PHASE_OOT_EXIT:
                    // #843 (IntGameplayRoundtripCapture): both games' captured
                    // frames must be on disk, and not all uniform, before PASS.
                    if (FrameCapture_VerifyRequested()) {
                        char capMsg[1024];
                        if (!FrameCapture_Verify(capMsg, sizeof(capMsg))) {
                            fprintf(stderr, "[FRAME-CAPTURE] FAIL: %s\n", capMsg);
                            fflush(stderr);
                            IntegrationTest_GameplayFail("frame capture lock");
                            return;
                        }
                        fprintf(stderr, "[FRAME-CAPTURE] PASS: %s\n", capMsg);
                    }
                    fprintf(stderr,
                            "[GP-TEST] PASS: %d round trip(s), warp, and door transition survived "
                            "%d live frames per phase\n",
                            IntegrationTest_GameplayCyclesDone(), cfg->framesPerPhase);
                    fflush(stderr);
                    IntegrationTest_SetGameplayPhase(GP_PHASE_DONE);
                    if (IntegrationTest_PairedFirstCrossing() && !IntegrationTest_PairedSkipCreation()) {
                        // A pass leaves no file behind (a failure keeps it for
                        // forensics; the next run erases it by its name).
                        Save_DeleteFile(kPfcSlot);
                        fprintf(stderr, "[PFC] PASS: the paired first crossing and its return; erased file %d\n",
                                kPfcSlot + 1);
                        fflush(stderr);
                    }
                    IntegrationTest_SignalBootComplete(GAME_OOT, "gameplay round-trip complete");
                    break;
                default:
                    break;
            }
        });

        // OoT-side watchdog: fail loudly (with state) instead of timing out
        // silently if an OoT-owned phase stops making progress. Budgeted in
        // WALL-CLOCK seconds (#376 item 4) so the diagnostic below is emitted
        // before the CTest/`timeout` kill — a frame budget lost that race.
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            if (Context_GetCurrentGame() == GAME_MM) {
                return;
            }
            GameplayPhase phase = IntegrationTest_GetGameplayPhase();
            switch (phase) {
                case GP_PHASE_BOOT:
                case GP_PHASE_OOT_PRE:
                case GP_PHASE_OOT_RETURN:
                case GP_PHASE_OOT_WARP:
                case GP_PHASE_OOT_EXIT:
                    break;
                default:
                    // Not an OoT-owned phase (MM is driving, or the run is
                    // done): re-base so the timer only measures the current
                    // OoT phase's stall, never the MM leg.
                    IntegrationTest_GameplayWatchdogRebase(&sGpWatchdogTimer, (int)phase, sGpFramesInPhase,
                                                           GpWatchdogNowSecs());
                    return;
            }
            double now = GpWatchdogNowSecs();
            GameplayWatchdogVerdict verdict = IntegrationTest_GameplayWatchdogTick(
                &sGpWatchdogTimer, (int)phase, sGpFramesInPhase, now, sGpWatchdogBudgetSecs);
            if (!sGpWatchdogFired && verdict != GP_WATCHDOG_OK) {
                sGpWatchdogFired = true;
                PlayState* play = OoT_gPlayState;
                bool stalled = verdict == GP_WATCHDOG_STALLED;
                fprintf(stderr,
                        "[GP-TEST] OoT watchdog: %s for %.1f s (%s %d s) in phase %d "
                        "(play=%p scene=%d entrance=0x%04X arrivedPhase=%d sceneInits=%d gameplayFrames=%d)\n",
                        stalled ? "no progress" : "phase still running",
                        now - (stalled ? sGpWatchdogTimer.progressSecs : sGpWatchdogTimer.phaseStartSecs),
                        stalled ? "budget" : "ceiling",
                        stalled ? sGpWatchdogBudgetSecs
                                : IntegrationTest_GameplayWatchdogCeilingSecs(sGpWatchdogBudgetSecs),
                        (int)phase, (void*)play, play ? play->sceneNum : -1, (uint16_t)gSaveContext.entranceIndex,
                        (int)sGpArrivalPhase, sGpSceneInits, sGpFramesInPhase);
                fflush(stderr);
                IntegrationTest_GameplayFail("OoT-side phase watchdog expired");
            }
        });

        fprintf(stderr, "[OoT] gameplay round-trip hooks registered\n");
        fflush(stderr);
    }
}

extern "C" {

int OoT_Game_Init(int argc, char** argv) {
    fprintf(stderr, "[OoT] Game_Init called, argc=%d\n", argc);
    fflush(stderr);

    // Store args for potential restart
    sArgc = argc;
    sArgv = argv;

    // Initialize OoT subsystems (matching what main() does)
    fprintf(stderr, "[OoT] Calling GameConsole_Init()...\n");
    fflush(stderr);
    GameConsole_Init();

    fprintf(stderr, "[OoT] Calling InitOTR()...\n");
    fflush(stderr);
    InitOTR(argc, argv);

    fprintf(stderr, "[OoT] Registering crash handler...\n");
    fflush(stderr);
    CrashHandlerRegisterCallback(CrashHandler_PrintSohData);

    fprintf(stderr, "[OoT] Calling BootCommands_Init()...\n");
    fflush(stderr);
    BootCommands_Init();

    fprintf(stderr, "[OoT] Calling OoT_Heaps_Alloc()...\n");
    fflush(stderr);
    OoT_Heaps_Alloc();

    // Register integration test hooks if in integration test mode
    OoT_RegisterIntegrationTestHooks();

    fprintf(stderr, "[OoT] Game_Init complete\n");
    fflush(stderr);
    return 0;
}

void OoT_Game_Run(void) {
    sOoTRunEntries++;
    fprintf(stderr, "[OoT] Game_Run called, entering Main()\n");
    fflush(stderr);
    // Run the main game loop
    Main(nullptr);
    fprintf(stderr, "[OoT] Main() returned\n");
    fflush(stderr);
}

// Shared cross-game resources (#525) — defined further down this TU, beside the
// apply half; forward-declared so Game_Suspend can harvest.
extern "C" void OoT_HarvestSharedResources(void);
// The combo tracker's departure capture (#1129), defined in
// soh/Enhancements/randomizer/TrackerAdapterSingleExe.cpp and declared in
// src/common's combo_tracker_view.h, which this TU does not include.
extern "C" void OoT_TrackerAdapter_CaptureAvailability(void);

/**
 * Suspend OoT for a game switch (issue #160).
 * Stops audio to prevent interference with MM, keeps libultraship context alive.
 */
void OoT_Game_Suspend(void) {
    fprintf(stderr, "[OoT] Game_Suspend called\n");
    fflush(stderr);

    // Producer (ADR 0002 / Lane A1): commit any staged cross-game items into
    // gComboCtx.sharedItemsTagged before we hand control to MM. This lives at
    // Game_Suspend — not Combo_CheckEntranceSwitch — on purpose: the F10
    // hot-swap path (Combo_FreezeActiveGameForHotSwap) bypasses the entrance
    // hook entirely, and GameRunner_SwitchTo calls suspend() on both switch
    // paths, so this is the one point that never drops a hotkey switch's
    // writes. The array itself is process-global and crosses the switch by
    // being shared; committing here just moves staged pickups into the durable
    // (serialized) store before the arriving game's consumer reads it.
    Combo_CommitStagedSharedItems();

    // Shared cross-game resources (#525): fold OoT's live rupees, wallet tier,
    // hearts, current health and double defense into the shared pool while
    // gSaveContext still belongs to OoT. Same reason this sits at Game_Suspend
    // rather than Combo_CheckEntranceSwitch — the F10 path bypasses the
    // entrance hook, and both switch paths call suspend().
    OoT_HarvestSharedResources();

    // The combo tracker's reachability capture (#1129, ruling D6): SoH's available
    // set as OoT departs, which the tracker's OoT pane shows while MM is played and
    // the crossing commit after this suspend writes to the .redsave.
    OoT_TrackerAdapter_CaptureAvailability();

    // Stop OoT audio playback to prevent interference with MM (issue #160).
    // OoT_Audio_PreNMI triggers the audio reset path which stops all sequences
    // and puts the audio system into a quiescent state.
    //
    // FIRST drain the OTR audio thread. It IS a real std::thread
    // (OTRGlobals.cpp OTRAudio_Init: audio.thread = std::thread(OTRAudio_Thread)),
    // NOT the commented-out N64 audioMgr thread — an earlier note here wrongly
    // claimed audio was synchronous. While it is mid-buffer it can load
    // sequences/soundfonts through the shared ResourceManager, so the switch's
    // archive hot-swap that follows suspend must not free a resource it is
    // using (that use-after-free corrupts the process heap and later faults in
    // RtlAllocateHeap). Wait for the in-flight buffer to finish before PreNMI.
    fprintf(stderr, "[OoT] Draining audio thread before suspend...\n");
    fflush(stderr);
    OoT_Audio_DrainForSuspend();

    fprintf(stderr, "[OoT] Stopping audio via PreNMI path...\n");
    fflush(stderr);
    OoT_Audio_PreNMI();

    // Mark audio as uninitialized so re-init works on resume
    gAudioContextInitalized = false;

    // Retire the graph coroutine: re-entering OoT runs Main() again, which
    // re-initializes the system arena (0xAB fill) underneath any suspended
    // gamestate — resuming the frame loop would update a poisoned PlayState
    // (the int-gameplay-roundtrip return-leg AV in GameState_SetFrameBuffer).
    // The next OoT entry cold-starts the gamestate chain; continuity is the
    // frozen SaveContext + the OoT-tagged startup entrance, which the title
    // screen fast-forwards on and OoT_Play_Init consumes.
    fprintf(stderr, "[OoT] Retiring graph coroutine for switch...\n");
    fflush(stderr);
    OoT_Graph_ResetRunFrameContext();

    // The Play gamestate just retired never ran Play_Destroy, so none of its
    // actors were deleted: put back what their deletion would have (#750).
    OoT_RetireAbandonedSession();

    fprintf(stderr, "[OoT] Game_Suspend complete\n");
    fflush(stderr);
}

/**
 * Undo what an ABANDONED OoT Play session left behind (#750, the OoT leg of
 * #666; MM's twin is MM_RetireAbandonedSession).
 *
 * Every departure from OoT retires its Play gamestate without Play_Destroy
 * (OoT_Graph_ResetRunFrameContext in OoT_Game_Suspend; the next entry re-runs
 * Main(), which re-initializes the system arena the actors lived in). So
 * OoT_Actor_Delete never runs for the actors that were live at that instant,
 * and two things its epilogue does are otherwise never done:
 *
 *  - each actor's ActorDB entry loses a client (numLoaded--), and the last
 *    client out runs the entry's `reset` (OoT_Actor_FreeOverlay), which is how
 *    the port puts overlay file-scope statics back. Unlike MM, OoT never zeroes
 *    numLoaded again after the entry is created (MM's Actor_InitContext does
 *    on every Play_Init), so without this an overlay with a live actor at
 *    departure keeps a phantom client for the rest of the process: its reset
 *    never runs again, even on ordinary in-OoT scene changes, and every further
 *    departure adds another phantom.
 *  - ObjectExtension_Free(actor) drops the per-actor data SoH hangs off actor
 *    addresses (the rando check identity of a pot, crate, grass blade, tree,
 *    beehive, fairy or scrub, the actor-list index, enemy maximum health). The
 *    next session's arena reuses those addresses.
 *
 * Both are done here, at the point the session is abandoned. An entry with
 * clients is marked client-free and its reset runs once, which is everything
 * OoT_Actor_FreeOverlay does at zero clients apart from a debug print that
 * reads HREG(20) through gGameInfo (calling reset directly keeps this
 * independent of that system-arena allocation). Entries already at zero
 * clients are skipped: they were reset when their last client went, and
 * skipping them keeps this seam equal to what OoT_Actor_FreeOverlay would
 * have done for the abandoned clients (a reset runs only for an overlay that
 * loses its last client). Every reset in the tree today is idempotent (plain
 * assignments, memsets, loops of assignments), so the skip is about matching
 * FreeOverlay and staying correct for a future reset that is not, not a
 * present hazard. The actors' own Destroy functions are NOT run: they take
 * the PlayState of a gamestate that has already been retired (dynapoly,
 * colliders, lights, skeletons). Overlays whose Destroy is what restores a
 * static got that restore in a reset of their own (the [RSBS #750] blocks
 * under games/oot/src/overlays/actors).
 *
 * The oot-abandoned-session-statics row (oot_abandoned_session_test.cpp) drives
 * OoT_GetGameOps()->suspend, so it fails if this call leaves OoT_Game_Suspend
 * or moves ahead of OoT_Graph_ResetRunFrameContext.
 */
void OoT_RetireAbandonedSession(void) {
    s32 overlays = 0;

    if (ActorDB::Instance != nullptr) {
        const int count = ActorDB::Instance->GetEntryCount();
        for (int i = 0; i < count; i++) {
            ActorDBEntry* entry = &ActorDB::Instance->RetrieveEntry(i).entry;
            if (entry->numLoaded <= 0) {
                continue;
            }
            entry->numLoaded = 0;
            if (entry->reset != NULL) {
                entry->reset();
            }
            overlays++;
        }
    }
    const size_t extensions = ObjectExtension::GetInstance().ClearAll();

    fprintf(stderr, "[OoT] Abandoned session retired: %d overlay(s) reset, %zu object extension entries dropped\n",
            (int)overlays, extensions);
    fflush(stderr);
}

/**
 * Resume OoT after being suspended for a game switch (issue #160, #170).
 * - Restores frozen OoT SaveContext so gameplay state survives the MM
 *   round-trip (MM scribbles over the unified gSaveContext storage while
 *   it is active — see src/common/unified_save.c).
 * - Reinitializes audio message queues for clean state.
 */
void OoT_Game_Resume(void) {
    fprintf(stderr, "[OoT] Game_Resume called\n");
    fflush(stderr);

    // Restore the frozen OoT SaveContext captured before we left for MM (#170).
    // Only restores when a frozen state exists — first boot of OoT skips this.
    if (Context_HasFrozenState(GAME_OOT)) {
        fprintf(stderr, "[OoT] Restoring frozen SaveContext on resume\n");
        fflush(stderr);
        Context_RestoreState(GAME_OOT, &gSaveContext, sizeof(gSaveContext));

        // Prefer an explicit startup entrance (set by main.cpp for this
        // switch); fall back to the return entrance recorded at freeze time.
        // Query the OoT-scoped accessor rather than the game-agnostic one: a
        // value tagged for MM (e.g. 0xC010) that leaked into the shared startup
        // global must NOT be applied to OoT's entranceIndex — it is a direct
        // linear index into gEntranceTable and would read far out of bounds
        // (crash) once Play_Init runs. When the leaked value is invisible to
        // OoT, hasStartup is false and we fall back to the frozen OoT return
        // entrance, which is the correct resume target and always in range.
        // Use the Has check rather than (entrance != 0) — entrance 0x0000 is the
        // real id for Kokiri Forest from Deku Tree, so a legit restore to 0 must
        // not be silently dropped. The frozen return entrance is always
        // trustworthy here because we already checked Context_HasFrozenState.
        bool hasStartup = Combo_HasStartupEntranceForGame("oot");
        uint16_t targetEntrance =
            hasStartup ? Combo_GetStartupEntranceForGame("oot") : Context_GetFrozenReturnEntrance(GAME_OOT);
        gSaveContext.entranceIndex = targetEntrance;
        fprintf(stderr, "[OoT] Resume entrance: 0x%04X (startup=%u)\n", targetEntrance, hasStartup);

        // NOTE: with the cold-boot contract this restore is defense in depth,
        // not the continuity mechanism — the resume fast-forward passes
        // through Opening_Init (z_opening.c), which re-authors the ADULT
        // debug save over gSaveContext AFTER this runs. The restore that
        // reaches gameplay (plus the force-child-on-return equip swap) lives
        // at the startup-entrance consumption point in OoT_Play_Init
        // (games/oot/src/code/z_play.c) — the first spot after the last wipe,
        // exactly mirroring MM_Play_ConsumeStartupEntrance.
    }

    // Reinitialize audio message queues for clean state (issue #160).
    // The audio context's queue pointers may be stale after suspend.
    fprintf(stderr, "[OoT] Reinitializing audio message queues...\n");
    fflush(stderr);
    Audio_InitMesgQueues();

    // Re-arm the shared audio thread's OoT synth and restart the sound
    // system (mirrors MM_Game_Resume). The audio heap survives suspend;
    // PreNMI only scheduled a reset and halted the players, and the
    // OoT_AudioMgr_Init bring-up is behind a never-re-run static guard
    // (audioMgr.c hasInitialized). Without this, every OoT return leg played
    // pure silence: the reset completed on the shared thread, no player ever
    // restarted, and the restored save's stale seqId told the scene its BGM
    // was already live (that last part is reset at the startup-entrance
    // consumption in z_play.c). InitSound's commands queue in the ring and
    // apply once the reset finishes.
    gAudioContextInitalized = true;
    OoT_Audio_ResumeFromPreNMI();
    OoT_Audio_InitSound();

    fprintf(stderr, "[OoT] Game_Resume complete\n");
    fflush(stderr);
}

/**
 * Full shutdown (final exit, no game switch coming).
 */
void OoT_Game_Shutdown(void) {
    fprintf(stderr, "[OoT] Game_Shutdown called\n");
    fflush(stderr);
    // #1349: the Item Tracker layout editor's write waits for edits to settle;
    // one made just before quitting is written here, while the config still is.
    ComboGui::ComboItemTrackerFlushLayoutSave(true);
    gAudioContextInitalized = false;
    DeinitOTR();
    OoT_Heaps_Free();
    fprintf(stderr, "[OoT] Game_Shutdown complete\n");
    fflush(stderr);
}

const char* OoT_Game_GetName(void) {
    return "Ocarina of Time";
}

const char* OoT_Game_GetId(void) {
    return "oot";
}

} // extern "C"

// ============================================================================
// GameOps registration
// ============================================================================

static GameOps sOoTOps = { "oot",           "Ocarina of Time", OoT_Game_Init, OoT_Game_Run, OoT_Game_Suspend,
                           OoT_Game_Resume, OoT_Game_Shutdown };

extern "C" GameOps* OoT_GetGameOps(void) {
    return &sOoTOps;
}

// ============================================================================
// Cross-game entrance hooks (single-exe mode)
// These were in GameExports.cpp but guarded out by #ifndef RSBS_SINGLE_EXECUTABLE.
// In single-exe mode, these are the REAL implementations called by game code.
// ============================================================================

// Cross-game entrance API (from src/common/)
extern "C" {
uint16_t Combo_CheckCrossGameEntrance(const char* gameId, uint16_t entrance);
bool Combo_IsCrossGameSwitch(void);
uint16_t Combo_GetSwitchReturnEntrance(void);
void Combo_FreezeState(const char* gameId, uint16_t returnEntrance, const void* saveCtx, size_t saveCtxSize);
void Combo_SignalReadyToSwitch(void);
void Combo_RequestGameSwitch(void);
bool Combo_IsGameSwitchRequested(void);
void Combo_ClearGameSwitchRequest(void);
// Hot-swap freeze policy (src/common/switch.cpp). The launcher drives the F10
// freeze, but only this side can supply the SaveContext, so the glue below
// bridges the two.
int Switch_PrepareHotSwap(GameId departing, const void* saveContext, size_t size);
}

// z_play.c. The matching prototype is in functions.h, which this TU does not
// include (it reaches z64.h through GameInteractor.h, not global.h).
extern "C" void Play_SaveSceneFlags(PlayState* play);

/**
 * OoT's half of Combo_FlushLiveStateForFreeze (#638): copy the current scene's
 * live flags into gSaveContext before the departure freezes it.
 *
 * OoT keeps the flags of the scene being played in play->actorCtx.flags and
 * copies them into gSaveContext.sceneFlags[play->sceneNum] only from
 * Actor_CleanupContext (z_actor.c) on a scene transition. Both cross-game
 * departures skip that copy: the entrance path freezes from inside
 * Play_Update's transition check (z_play.c, before the transition runs) and
 * then stops the gamestate with init/destroy nulled, and the F10 path breaks the
 * graph loop with the PlayState still live and never destroys it. Everything
 * set during the final scene visit -- a chest, a switch, a collectible -- was
 * frozen as unset and the return leg restored it that way. MM has the same
 * shape (MM_Combo_FlushSceneFlagsForFreeze) and, unlike OoT's Market, a
 * persistent collectible right at its portal, which is how #635 surfaced it.
 *
 * Play_SaveSceneFlags is the exact copy Actor_CleanupContext would have made:
 * four u32 words, idempotent, indexed by the live sceneNum. NULL-safe: the
 * launcher's freeze can run with no PlayState (a switch requested from outside
 * gameplay, or the headless rows), and OoT_gPlayState is nulled by Play_Destroy
 * and OoT_Graph_ResetRunFrameContext, so a non-NULL value is a live PlayState.
 */
extern "C" void OoT_Combo_FlushSceneFlagsForFreeze(void) {
    PlayState* play = OoT_gPlayState;
    if (play == NULL) {
        return;
    }
    Play_SaveSceneFlags(play);
}

/**
 * OoT's half of the resume position a crossing commit carries (#850): record
 * the scene Link departs from as the save's scene, as a save made there would.
 *
 * Since #837 every crossing commits the departure freeze to the .redsave, and a
 * reload boots OoT through OoT_Sram_OpenSave, which places Link by
 * gSaveContext.savedSceneNum (z_sram.c): a dungeon or boss scene -> that
 * dungeon's entrance, Ganon's Tower collapse -> the tower, a grotto or fairy
 * fountain -> Link's House / the Temple of Time even with Remember Save Location
 * on, any other scene -> Link's House / the Temple of Time, or with Remember
 * Save Location the frozen entranceIndex. Under entrance shuffle the
 * randomizer's OnLoadGame handler (Entrance_SetSavewarpEntrance) keys on the
 * same field after it. Only a save writes it (Play_PerformSave, the kaleido
 * save prompt), so without this the freeze carried the scene of the LAST SAVE:
 * an F10 crossing taken inside a grotto, a fountain, a boss room or the
 * collapse, reloaded with Remember Save Location, woke Link inside that
 * interior, which SoH's own save never does. OoTMM's comboGameSwitch does the
 * same at an OoT switch (its Save_DoSave sets the save's scene to the current
 * one).
 *
 * The guard is Play_PerformSave's own: a live PlayState and a real file
 * (fileNum != 0xFF; a debug save is never saved). No PlayState (the headless
 * rows, a switch requested outside gameplay): nothing to read, the last save's
 * scene is kept. A cross-game ARRIVAL never reads the field: it is placed by its
 * startup entrance (z_play.c), and OnLoadGame is dispatched only by the file
 * select's load (z_file_choose.c), SoH's boot-to-warp-point debug save and the
 * integration drives. Idempotent: the launcher's re-freeze stamps the same scene.
 *
 * The committed-transition frame. Play_Update commits an in-OoT scene change by
 * clearing state.running, SET_NEXT_GAMESTATE(OoT_Play_Init) and writing
 * gSaveContext.entranceIndex = nextEntranceIndex (z_play.c, all four sites);
 * the PlayState is destroyed only by the NEXT OoT_RunFrame, and
 * OoT_Graph_ThreadEntry polls Combo_CheckHotSwap first (graph.c). An F10 on
 * that one poll freezes a PlayState whose sceneNum is the scene Link is
 * LEAVING against an entranceIndex that already points into the next one, so
 * the stamp records the scene that entranceIndex resumes in. The cross-game
 * door is not this case: it freezes before SET_NEXT_GAMESTATE, with
 * entranceIndex still the departure scene's (z_play.c), so init is not
 * OoT_Play_Init there.
 */
extern "C" EntranceInfo gEntranceTable[ENTR_MAX]; // games/oot/include/variables.h
extern "C" void OoT_Combo_StampSavedSceneForFreeze(void) {
    PlayState* play = OoT_gPlayState;
    if (play == NULL || gSaveContext.fileNum == 0xFF) {
        return;
    }
    s16 scene = play->sceneNum;
    const char* why = "the scene Link departs from";
    if (!play->state.running && play->state.init == (GameStateFunc)OoT_Play_Init && gSaveContext.entranceIndex >= 0 &&
        gSaveContext.entranceIndex < ENTR_MAX) {
        scene = gEntranceTable[gSaveContext.entranceIndex].scene;
        why = "the scene of the transition committed this frame";
    }
    if (gSaveContext.savedSceneNum != scene) {
        fprintf(stderr, "[OoT] pre-freeze: savedSceneNum %d -> %d, %s (#850)\n", (int)gSaveContext.savedSceneNum,
                (int)scene, why);
    }
    gSaveContext.savedSceneNum = scene;
}

// z_actor.c. functions.h is not included here (see Play_SaveSceneFlags above);
// these are the exact entry points the two Destroys below call, so the flag
// hooks (check tracker, network sync) see the same dispatch a scene exit gives.
extern "C" s32 OoT_Flags_GetEventChkInf(s32 flag);
extern "C" void OoT_Flags_SetEventChkInf(s32 flag);
extern "C" void Flags_UnsetEventChkInf(s32 flag);
// #807: what OoT_ObjLightswitch_Destroy and Play_Destroy call (z_actor.c,
// z_parameter.c), and SoH's Sunlight Arrows static (z_obj_lightswitch.c).
extern "C" void OoT_Flags_UnsetSwitch(PlayState* play, s32 flag);
extern "C" void Inventory_SwapAgeEquipment(void);
extern "C" bool sunSwitchActivatedByLightArrow;

/**
 * OoT's half of the pre-freeze discipline for actor Destroys whose writes are
 * "on any exit from this scene" (#770, the residue of #750 / PR #767).
 *
 * Both cross-game departures retire OoT's Play gamestate without Play_Destroy,
 * so no actor Destroy runs for the actors live at that instant (see
 * OoT_RetireAbandonedSession, which puts back overlay statics and ActorDB client
 * counts, and deliberately does not run Destroys). Two Destroys write the save
 * UNCONDITIONALLY on scene exit, so an F10 (or a cross-game door) froze the save
 * as if the player had never left:
 *
 *  - Bg_Relay_Objects (z_bg_relay_objects.c, BgRelayObjects_Destroy): the
 *    windmill's rotating gear unsets EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL
 *    whenever gSaveContext.cutsceneIndex < 0xFFF0. The flag is what makes the
 *    gear and Kakariko's windmill sails (Bg_Spot01_Fusya) spin fast; vanilla
 *    clears it on every non-cutscene exit from the windmill.
 *  - Bg_Spot06_Objects (z_bg_spot06_objects.c, BgSpot06Objects_Destroy): every
 *    Lake Hylia object, in rando, sets EVENTCHKINF_RAISED_LAKE_HYLIA_WATER once
 *    EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP is set (the water-control switch
 *    lowers the lake only for the visit). The same Destroy writes the lower
 *    river water box's zMin back on the resource-cached scene collision header
 *    unconditionally, because Init subtracts 50 from it whenever the lake loads
 *    lowered; that is the non-save half of the same skip and is applied here too.
 *
 * WHY HERE AND NOT AT OoT_Game_Suspend. The blob is captured BEFORE suspend on
 * both paths (Combo_CheckEntranceSwitch and Combo_FreezeActiveGameForHotSwap,
 * each right after Combo_FlushLiveStateForFreeze); a write at suspend or in
 * OoT_RetireAbandonedSession would change the live gSaveContext after the
 * freeze and never reach the frozen half. This runs inside that flush, where
 * the PlayState and its actor lists are still live.
 *
 * WHICH ACTORS. The live actor lists are walked, so the condition is the
 * Destroy's own, evaluated on the actors that would have been destroyed: an
 * actor with a destroy function (OoT_Actor_Destroy calls only a non-NULL one),
 * the gear by its post-Init params (WINDMILL_ROTATING_GEAR, 0; a duplicate gear
 * is killed with params 0xFF and its Destroy writes nothing), any Lake Hylia
 * object for the lake. Scene numbers are not consulted: the actors are what the
 * Destroys key on. No vendored Destroy is touched; an ordinary in-OoT scene exit
 * still runs them and never reaches this function, which only the cross-game
 * freeze seam calls. Idempotent (an unset, a set and an assignment). NULL-safe
 * on OoT_gPlayState like the scene-flag flush: no live PlayState, nothing to do.
 *
 * #807 adds the two residues PR #792 recorded:
 *
 *  - Obj_Lightswitch (z_obj_lightswitch.c, OoT_ObjLightswitch_Destroy): with
 *    SoH's Sunlight Arrows, a sun switch lit by a Light Arrow sets the overlay
 *    static sunSwitchActivatedByLightArrow, and the Destroy then unsets that
 *    switch's flag on any exit (the enhancement makes the activation last one
 *    visit). Only the first non-BURN switch destroyed acts: it unsets its flag
 *    unless it is in room 25 (the Spirit Temple chain platform stays down) and
 *    clears the static; a BURN switch writes nothing and keeps the static. The
 *    walk below is Actor_CleanupContext's destroy order (categories ascending,
 *    each list from its head), so the same switch acts. The unset lands on the
 *    live play flags; Combo_FlushLiveStateForFreeze therefore runs this seam
 *    BEFORE the #638 scene-flag flush, the order Actor_CleanupContext uses
 *    (every Destroy, then Play_SaveSceneFlags).
 *  - Player (z_player.c, OoT_Player_Destroy): gSaveContext.linkAge =
 *    play->linkAgeOnLoad. Play_Destroy, just before it destroys the actors,
 *    swaps the age equipment when the two differ (Inventory_SwapAgeEquipment
 *    archives the CURRENT age's equips, so it must run before the write). They
 *    differ only between a linkAgeOnLoad write (the Temple of Time age change
 *    in z_demo.c, SoH's switch-age mod) and the reload it triggers. Without the
 *    swap the frozen save would wear the old age's equipment, and the arrival's
 *    force-child guard (z_play.c) skips its own swap when linkAge already reads
 *    child. Both writes are applied only while a Player with a destroy function
 *    is live: no Player, no Destroy, and the swap never runs without the write.
 *
 * The session-only rows of #770 are normalised on re-entry (see that issue).
 */
extern "C" void OoT_Combo_ApplySceneExitWritesForFreeze(void) {
    PlayState* play = OoT_gPlayState;
    if (play == NULL) {
        return;
    }
    // Values private to the overlays' .c files: WindmillSetpiecesMode's
    // WINDMILL_ROTATING_GEAR, LakeHyliaWaterBoxIndices'
    // LHWB_GERUDO_VALLEY_RIVER_LOWER, WATER_LEVEL_RIVER_LOWER_Z, and
    // ObjLightswitch_Type's OBJLIGHTSWITCH_TYPE_BURN.
    const s16 kWindmillRotatingGear = 0;
    const s32 kRiverLowerWaterBox = 1;
    const s16 kRiverLowerZMin = 2203;
    const s16 kSunSwitchTypeBurn = 3;
    const s8 kSunSwitchChainPlatformRoom = 25;

    bool windmillGear = false;
    bool lakeObjects = false;
    bool player = false;
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            if (actor->destroy == NULL) {
                continue;
            }
            if (actor->id == ACTOR_BG_RELAY_OBJECTS && actor->params == kWindmillRotatingGear) {
                windmillGear = true;
            } else if (actor->id == ACTOR_BG_SPOT06_OBJECTS) {
                lakeObjects = true;
            } else if (actor->id == ACTOR_PLAYER) {
                player = true;
            } else if (actor->id == ACTOR_OBJ_LIGHTSWITCH && sunSwitchActivatedByLightArrow &&
                       ((actor->params >> 4) & 3) != kSunSwitchTypeBurn) {
                if (actor->room != kSunSwitchChainPlatformRoom) {
                    OoT_Flags_UnsetSwitch(play, (actor->params >> 8) & 0x3F);
                    fprintf(stderr,
                            "[OoT] pre-freeze: sun switch lit by a Light Arrow turned off, switch 0x%02X (#807)\n",
                            (actor->params >> 8) & 0x3F);
                }
                sunSwitchActivatedByLightArrow = false;
            }
        }
    }

    if (player && gSaveContext.linkAge != play->linkAgeOnLoad) {
        const s32 frozenAge = gSaveContext.linkAge;
        Inventory_SwapAgeEquipment();
        gSaveContext.linkAge = play->linkAgeOnLoad;
        fprintf(stderr, "[OoT] pre-freeze: pending age change applied, linkAge %d -> %d (#807)\n", (int)frozenAge,
                (int)gSaveContext.linkAge);
    }

    if (windmillGear && gSaveContext.cutsceneIndex < 0xFFF0 &&
        OoT_Flags_GetEventChkInf(EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL)) {
        Flags_UnsetEventChkInf(EVENTCHKINF_PLAYED_SONG_OF_STORMS_IN_WINDMILL);
        fprintf(stderr, "[OoT] pre-freeze: windmill gear exit cleared the Song of Storms windmill flag (#770)\n");
    }

    if (lakeObjects) {
        CollisionHeader* colHeader = play->colCtx.colHeader;
        if (colHeader != NULL && colHeader->waterBoxes != NULL && colHeader->numWaterBoxes > kRiverLowerWaterBox) {
            colHeader->waterBoxes[kRiverLowerWaterBox].zMin = kRiverLowerZMin;
        }
        if (IS_RANDO && OoT_Flags_GetEventChkInf(EVENTCHKINF_USED_WATER_TEMPLE_BLUE_WARP) &&
            !OoT_Flags_GetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER)) {
            OoT_Flags_SetEventChkInf(EVENTCHKINF_RAISED_LAKE_HYLIA_WATER);
            fprintf(stderr, "[OoT] pre-freeze: Lake Hylia exit raised the water again after the Water Temple (#770)\n");
        }
    }
}

/**
 * Freeze the departing game for an F10 hot swap (#364).
 *
 * The launcher (rsbs/src/main.cpp) decides that a hot swap is happening, but
 * `gSaveContext` is only addressable from a game translation unit — hence this
 * one-line bridge. It is the F10 twin of the freeze `Combo_CheckEntranceSwitch`
 * performs below for the entrance path; without it, F10 departures produced no
 * blob at all while the launcher still restored whatever blob an *earlier*
 * entrance switch had left behind.
 *
 * As in `Combo_CheckEntranceSwitch`, `sizeof(gSaveContext)` here is OoT's
 * layout even when MM is the departing game. That over-reads relative to MM's
 * smaller struct but stays in bounds: the underlying unified storage
 * (src/common/unified_save.c) is OOT_SAVE_CONTEXT_SIZE for both games, and
 * Context_FreezeState clamps to the per-game blob capacity.
 *
 * @return 1 if a fresh blob was recorded, 0 if the launcher must refuse the
 *         switch instead of proceeding into a stale restore.
 */
extern "C" int Combo_FreezeActiveGameForHotSwap(GameId departing) {
    // Pre-freeze discipline (#638, #626, #664): the live scene flags and a
    // dead health bar (both games, #626/#664) must be folded into gSaveContext BEFORE the bytes are
    // captured. This is the one point on the F10 path that passes the real
    // gSaveContext, so the flush lives here rather than inside
    // Switch_PrepareHotSwap (which src/common tests drive with scratch buffers).
    Combo_FlushLiveStateForFreeze(departing);
    return Switch_PrepareHotSwap(departing, &gSaveContext, sizeof(gSaveContext));
}

/**
 * Award a single OoT-origin shared item (ADR 0002 / Lane A1 consumer callback,
 * real give wired by Lane C1 #392).
 *
 * Invoked once per un-redeemed entry tagged GAME_OOT when the player arrives in
 * OoT (see OoT_ConsumeSharedItems). `item->id` is an OoT RandomizerGet (RG_*).
 *
 * The give itself lives in OoT_ForeignItem_Give
 * (soh/Enhancements/randomizer/ForeignItemsSingleExe.cpp): GetGIEntry
 * resolution (progressives resolve against the live save) + the
 * StartingItemGive-style dispatch into OoT_Item_Give / Randomizer_Item_Give.
 * Combo_RedeemSharedItemsForGame marks the entry RSBS_SHARED_ITEM_REDEEMED
 * after this returns, so the crossing stays single-use; the entry is never
 * cleared (the durable record of the crossing). A give that reports failure is
 * logged but still consumes the redemption — by the time this callback can
 * run, OoT is at its presence-gated arrival point and the guarded
 * prerequisites are live, so that path is defensive, not expected.
 *
 * ONE ANNOUNCEMENT PER ITEM (#878). A crossing pickup (RSBS_SHARED_ITEM_CROSSING:
 * an OoT item taken from a Majora's Mask check) was presented when the player
 * took it, by MM's get-item textbox or a shop's own purchase dialogue, so this
 * arrival gives it silently: toasting it here announced the same item a second
 * time at the next crossing into OoT, the operator's "duplicate toasts when you
 * cross over". The paired world is one game, and OoTMM (the reference for
 * player-facing choices) presents an item once, at its pickup, and announces
 * nothing at a game switch. Only a SOURCED grant (RSBS_SHARED_ITEM_SOURCED, the
 * netplay relay's Combo_SubmitSourcedGrant) keeps its toast: it was never picked
 * up in this player's game, so the toast is its only presentation. The redeem
 * and its log line are the same for both. Locked by CrossingArrivalToasts and by
 * IntPairedFirstCrossing's per-leg toast check.
 */
static void OoT_AwardSharedItem(const SharedItem* item, void* ctx) {
    (void)ctx;
    // #1061: a counted pack (an OoT bombchu pack an older save crossed from
    // Termina) is credited to the shared pool, not given: OoT's give adds to
    // OoT's own count under OoT's own cap, which loses what the cap cuts.
    // New pickups never cross such a pack (OoT_/MM_CreditSharedPickup at the
    // pickup); this is the redemption of the ones recorded before that.
    int given = OoT_CreditSharedPickup(item) != 0 ? 1 : OoT_ForeignItem_Give(item->id);
    fprintf(stderr, "[OoT] shared-item redeem: RG id=%u %s (Lane C1 foreign give)\n", (unsigned)item->id,
            given ? "awarded" : "NOT awarded — give path unavailable");
    if ((item->flags & RSBS_SHARED_ITEM_SOURCED) == 0) {
        return; // #878: presented at its pickup (see above)
    }

    // #494: tell the player. Until this, a cross-game item arrived in OoT with
    // NO in-game signal at all — the item simply appeared in the inventory, an
    // arbitrary number of scenes after the MM check that granted it. The MM
    // side has said "it will be awarded there!" since Lane C1; this is the
    // other half of that sentence.
    //
    // A toast rather than a textbox on purpose: the redeem runs inside
    // Play_Init (z_play.c:565), before the first frame, where there is no
    // message context to drive and nothing to dismiss it with. The notification
    // overlay is the one surface that works from here, and it is also the
    // surface the arrival ALREADY renders over — MM's rando pickups cross-bind
    // to this exact Emit (#427 item 1).
    //
    // .itemIcon comes from Combo_GetForeignItemIconName (#494): the pinned pool
    // now carries each OoT item's ITEM_* texture-map key alongside its name and
    // article, and the accessor serves it back origin-keyed, so src/common never
    // translates an RG_* into a texture. It may still return NULL (an entry with
    // no icon, or an id outside the pool), and a NULL icon is text-only, not a
    // degradation — the MOD_RANDOMIZER branch in hook_handlers.cpp omits the icon
    // too. The overlay stores the pointer and dereferences it at draw time, well
    // after the item icons are registered, and the pool's key is a string literal
    // that outlives the toast.
    //
    // WORDING (#510): "You got the Fairy Bow", not "Received from Termina: …".
    // The arrival IS the moment the player receives it in OoT, so the native
    // sentence is the correct one; naming the other game was the cross-game
    // tell the operator asked to remove. Name and article both come from the
    // pinned pool via the origin-tagged SharedItem, so this never fabricates an
    // id-space crossing; both return NULL when that origin's pool is not linked
    // into this build, hence the fallbacks.
    //
    // FIELD ARRANGEMENT (#494): verb in `.message`, item name in `.suffix` —
    // the arrangement every OTHER OoT rando pickup toast uses
    // (Enhancements/randomizer/hook_handlers.cpp). Options colours each field
    // differently, so the earlier `.prefix` + `.message` form rendered a foreign
    // arrival in blue-then-grey where an ordinary pickup is grey-then-red. That
    // difference is visible at a glance and reads as "this one is special",
    // which is the class of tell #510 removed from the model and the text.
    const char* foreignName = Combo_GetForeignItemName(*item);
    const char* foreignArticle = Combo_GetForeignItemArticle(*item);
    Notification::Emit({
        .itemIcon = Combo_GetForeignItemIconName(*item),
        .message = "You got ",
        .suffix = std::string(foreignArticle != nullptr ? foreignArticle : "") +
                  (foreignName != nullptr ? foreignName : "a foreign item"),
    });
}

// ---- Skip Epona Race's Epona (#1351) ----------------------------------------
//
// Since #1327 (SoH 29b9b4a5a) Skip Epona Race no longer sets
// EVENTCHKINF_EPONA_OBTAINED at creation. OoT's item-receive handler sets it
// when Epona's Song is given for a QUEUED CHECK (hook_handlers.cpp,
// RandomizerOnItemReceiveHandler, which returns first when none is queued). The
// song also reaches OoT with no check queued: the Shared Epona's Song apply
// (OoT_SharedSongGive), an OoT Epona's Song redeemed from a Majora's Mask check
// (OoT_ConsumeSharedItems) and Start with Epona's Song (savefile.cpp's
// OoT_Item_Give at creation). Every summon reads the flag (z_horse.c,
// z_player.c), so without it the song never calls Epona. Under Skip Epona Race,
// knowing the song IS having Epona: the option's own promise.

static bool OoT_SkipEponaRaceOn(void) {
    if (!IS_RANDO) {
        return false;
    }
    const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    return ctx != nullptr && ctx->GetOption(RSK_SKIP_EPONA_RACE).Get() != RO_GENERIC_DONT_SKIP;
}

// Run at the load seam (OoT_ApplySharedResourcesAtFileLoad), at every arrival
// (OoT_ConsumeSharedItems, the arrival's last give) and by the Shared Epona's
// Song give. Idempotent. Written straight into the save, as the family gives
// write theirs: Flags_SetEventChkInf would also fire the flag-set hooks, and
// both seams run before gameplay.
static void OoT_ReconcileEponaObtained(void) {
    if (!OoT_SkipEponaRaceOn() || !CHECK_QUEST_ITEM(QUEST_SONG_EPONA) || GET_EVENTCHKINF(EVENTCHKINF_EPONA_OBTAINED)) {
        return;
    }
    SET_EVENTCHKINF(EVENTCHKINF_EPONA_OBTAINED);
    fprintf(stderr, "[OoT] Skip Epona Race: Epona's Song is known, so Epona is obtained (#1351)\n");
}

/**
 * Consumer hook (ADR 0002 / Lane A1). Award every un-redeemed OoT-origin shared
 * item and mark it redeemed. Called from OoT_Play_Init's presence-gated
 * startup-entrance consumption (games/oot/src/code/z_play.c) — i.e. only on a
 * cross-game arrival into OoT, once per arrival. A plain boot / .redsave load
 * never reaches it, so un-redeemed items wait for the next switch into OoT
 * ("applies on next switch only"; see shared_items.h).
 *
 * Exposed as a plain C entry point so z_play.c does not have to pull in the
 * ADR SharedItem / GameId types (it isolates every src/common call behind bare
 * externs).
 */
extern "C" void OoT_ConsumeSharedItems(void) {
    Combo_RedeemSharedItemsForGame(GAME_OOT, OoT_AwardSharedItem, nullptr);
    // #1351: after every give of the arrival (the apply runs first, z_play.c).
    OoT_ReconcileEponaObtained();
}

// ============================================================================
// Shared cross-game RESOURCES — OoT side (#525)
//
// The merge rules live in src/common/shared_resources.c, which has no game
// headers. This is the half that does: it reads and writes OoT's own
// gSaveContext fields and converts them into the units the pool is defined in.
// MM's twin is games/mm/2s2h/GameExports_SingleExe.cpp.
// ============================================================================

// OoT's own upgrade setter and the three tables CUR_UPG_VALUE / CUR_CAPACITY
// expand to (games/oot/src/code/z_inventory.c, declared in variables.h).
// Declared here rather than by including variables.h: this TU deliberately keeps
// a narrow game-header surface — it already hand-declares gSaveContext for the
// same reason — and pulling in the full variable set to reach three arrays would
// widen it for no benefit. The declarations match variables.h exactly, so a
// layout change breaks the link rather than reading garbage.
extern "C" void OoT_Inventory_ChangeUpgrade(s16 upgrade, s16 value);
extern "C" u32 OoT_gUpgradeMasks[8];
extern "C" u8 OoT_gUpgradeShifts[8];
extern "C" u16 OoT_gUpgradeCapacities[8][4];
// Backs the SLOT/AMMO/INV_CONTENT macros (macros.h) the ammo shim uses.
extern "C" u8 OoT_gItemSlots[56];

// Declared locally because OTRGlobals.h only declares this inside its
// `#ifndef __cplusplus` block — the C half of the header — so it is invisible
// to every C++ TU. Redeclaring it here is the established pattern rather than a
// workaround: draw.cpp, CosmeticsEditor.cpp, NoMasterSword.cpp and several
// other C++ callers each do exactly this. The definition (OTRGlobals.cpp) is
// extern "C", which is the linkage this must match.
extern "C" u8 Randomizer_GetSettingValue(RandomizerSettingKey randoSettingKey);

// Heart pieces live in the TOP NIBBLE of questItems in BOTH games (OoT writes
// `1 << (QUEST_HEART_PIECE + 4)`, MM's QUEST_HEART_PIECE_COUNT is 0x1C), which
// is what makes one canonical quantity possible at all.
#define OOT_HEART_PIECE_SHIFT 28u
#define OOT_HEART_PIECE_MASK 0xF0000000u

// The highest wallet tier this build defines (OoT_gUpgradeCapacities row
// UPG_WALLET is {99, 200, 500, 999}). Passed as the apply CAP so a pool value
// authored by a future build with more tiers cannot index off the end of OoT's
// capacity table.
#define OOT_MAX_WALLET_TIER 3u

// Highest tier index in every ammo row of OoT_gUpgradeCapacities (each row has
// 4 entries). Passed as the apply CAP for the same reason OOT_MAX_WALLET_TIER
// is: a pool value authored by a future build with more tiers must not index
// off the end of the capacity table.
#define OOT_MAX_AMMO_TIER 3u

// OoT's VANILLA bombchu ceiling. Unlike arrows and bombs, bombchus have no
// upgrade row in either game — OoT fixes the cap at 50 while MM derives it from
// the bomb bag — so the two ends of this shared count clamp against different
// numbers, which is exactly the asymmetry the watermark discipline absorbs.
//
// Only half the story under rando: see OoT_BombchuCapacity.
#define OOT_BOMBCHU_CAPACITY 50u

// One shared ammo pair: the capacity tier, the count it bounds, and the
// inventory item a tier of 1 or more implies. Table-driven because the five
// rows differ only in these four values, and a copy-pasted block per row is
// how one of them silently ends up reading another's slot.
typedef struct {
    uint8_t tierKind;  // RSBS_SHARED_RES_*_TIER, or RSBS_SHARED_RES_NONE when the count has no tier (bombchus)
    uint8_t countKind; // RSBS_SHARED_RES_*_COUNT
    s16 upgrade;       // UPG_* row index, ignored when tierKind is NONE
    uint8_t item;      // ITEM_* whose slot holds the count, and which a nonzero tier grants
} OoTSharedAmmo;

static const OoTSharedAmmo kOoTSharedAmmo[] = {
    { RSBS_SHARED_RES_QUIVER_TIER, RSBS_SHARED_RES_ARROW_COUNT, UPG_QUIVER, ITEM_BOW },
    { RSBS_SHARED_RES_BOMB_BAG_TIER, RSBS_SHARED_RES_BOMB_COUNT, UPG_BOMB_BAG, ITEM_BOMB },
    { RSBS_SHARED_RES_STICK_TIER, RSBS_SHARED_RES_STICK_COUNT, UPG_STICKS, ITEM_STICK },
    { RSBS_SHARED_RES_NUT_TIER, RSBS_SHARED_RES_NUT_COUNT, UPG_NUTS, ITEM_NUT },
    { RSBS_SHARED_RES_NONE, RSBS_SHARED_RES_BOMBCHU_COUNT, 0, ITEM_BOMBCHU },
};

static uint16_t OoT_ReadAmmo(uint8_t item) {
    const s8 held = AMMO(item);
    return held < 0 ? 0u : (uint16_t)held;
}

/**
 * OoT's LIVE bombchu ceiling, which is 50 only outside the randomizer.
 *
 * With RSK_BOMBCHU_BAG set to progressive, SoH replaces the ceiling entirely:
 * Bombchus.cpp registers a VB_CHECK_BOMBCHU_CAPACITY override that CLAMPS
 * AMMO(ITEM_BOMBCHU) down to gRandoContext->GetBombchuCapacity() (0/20/30/50 by
 * bag level) on the next capacity check. Applying the shared pool against a
 * hardcoded 50 therefore does not just overfill — it stages a silent theft:
 * the clamp fires on the very next bombchu thrown, the count drops to the real
 * capacity, and the following harvest reads that as the player having SPENT the
 * difference and debits the shared pool by it. With bag level 0 the pool is
 * zeroed outright, so bombchus the player earned in Termina disappear from both
 * games at once.
 *
 * Mirrors the capacity expression in Bombchus.cpp rather than paraphrasing it,
 * including its restriction to the progressive option (the single-bag and
 * vanilla options leave the 50 in place and register no clamp).
 */
static uint16_t OoT_BombchuCapacity(void) {
    if (IS_RANDO && OTRGlobals::Instance != nullptr && OTRGlobals::Instance->gRandoContext != nullptr &&
        Randomizer_GetSettingValue(RSK_BOMBCHU_BAG) == RO_BOMBCHU_BAG_PROGRESSIVE) {
        return (uint16_t)OTRGlobals::Instance->gRandoContext->GetBombchuCapacity();
    }
    return (uint16_t)OOT_BOMBCHU_CAPACITY;
}

// The count's ceiling in OoT right now: the live capacity for a row that has an
// upgrade, the live bombchu capacity for the one that does not.
static uint16_t OoT_AmmoCapacity(const OoTSharedAmmo* row) {
    if (row->tierKind == RSBS_SHARED_RES_NONE) {
        return OoT_BombchuCapacity();
    }
    return (uint16_t)CUR_CAPACITY(row->upgrade);
}

// Put `item` in its own inventory slot if the slot is empty.
//
// This is the half of a tier grant OoT does NOT do for itself. Its tier-2 and
// tier-3 gives (Bigger Quiver, Bigger Bomb Bag) only widen the capacity, on the
// assumption that whatever granted tier 1 already put the bow or the bombs in
// the inventory. A cross-game apply breaks that assumption — the pool can hand
// OoT a quiver tier it never earned locally — so without this the player gets
// capacity and ammo with no usable C-item. MM's own gives set INV_CONTENT
// unconditionally, which is the behavior being matched.
static void OoT_EnsureInventoryItem(uint8_t item) {
    if (INV_CONTENT(item) == ITEM_NONE) {
        INV_CONTENT(item) = item;
    }
}

// ---- Shared families (#976's OR discipline): OoT's own copies ---------------
//
// Each family is one item both games have, made ONE by a frozen cross-game rule
// (foreign_items.h, RSBS_XITEM_FAMILY_MASK). OoT registers its NATIVE copy: the
// id the bag pairs (OoT's pool places it as this RG_*) and the two callbacks the
// harvest and apply shims call (shared_resources.h, SHARED FAMILIES). The give
// runs at the startup entrance, before gameplay, so it writes the inventory the
// way Item_Give does and shows nothing.

// #1065: the Lens of Truth (OoTMM sharedLens). Item_Give(ITEM_LENS) only fills
// its slot; nothing else in OoT's save records owning it.
static bool OoT_SharedLensHeld(uint16_t nativeId) {
    (void)nativeId;
    return INV_CONTENT(ITEM_LENS) == ITEM_LENS;
}
static void OoT_SharedLensGive(uint16_t nativeId) {
    (void)nativeId;
    INV_CONTENT(ITEM_LENS) = ITEM_LENS;
}

// #1066: Epona's Song and the Song of Storms (OoTMM sharedSongEpona,
// sharedSongStorms). Item_Give(ITEM_SONG_*) only sets the song's quest bit, and
// the ocarina staff builds its playable set from the quest bits alone
// (z_message_PAL.c, the CHECK_QUEST_ITEM(QUEST_SONG_MINUET + i) loop), so the
// bit is the whole of knowing the song.
static uint32_t OoT_SharedSongQuestBit(uint16_t nativeId) {
    return (nativeId == (uint16_t)RG_SONG_OF_STORMS) ? (1u << QUEST_SONG_STORMS) : (1u << QUEST_SONG_EPONA);
}
static bool OoT_SharedSongHeld(uint16_t nativeId) {
    return (gSaveContext.inventory.questItems & OoT_SharedSongQuestBit(nativeId)) != 0u;
}
static void OoT_SharedSongGive(uint16_t nativeId) {
    gSaveContext.inventory.questItems |= OoT_SharedSongQuestBit(nativeId);
    if (nativeId == (uint16_t)RG_EPONAS_SONG) {
        OoT_ReconcileEponaObtained(); // #1351: the song learnt in Termina calls Epona here too
    }
}

// #1069: the Song of Time (OoTMM sharedSongTime). Item_Give(ITEM_SONG_TIME) only
// sets the song's quest bit, and the ocarina staff builds its playable set from
// the quest bits alone (z_message_PAL.c, the CHECK_QUEST_ITEM(QUEST_SONG_MINUET +
// i) loop), so the bit is the whole of knowing the song.
static bool OoT_SharedSongOfTimeHeld(uint16_t nativeId) {
    (void)nativeId;
    return CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0;
}
static void OoT_SharedSongOfTimeGive(uint16_t nativeId) {
    (void)nativeId;
    gSaveContext.inventory.questItems |= (1u << QUEST_SONG_TIME);
}

// #968: the Fire, Ice and Light Arrows (OoTMM sharedMagicArrowFire, Ice,
// Light). Item_Give(ITEM_ARROW_*) only fills the arrow's own slot (z_parameter.c
// has no branch for them, so they take the plain INV_CONTENT write); the bow
// they ride is already one shared quiver tier (#744) and is not touched here.
static uint8_t OoT_SharedArrowItem(uint16_t nativeId) {
    return (nativeId == (uint16_t)RG_ICE_ARROWS)     ? (uint8_t)ITEM_ARROW_ICE
           : (nativeId == (uint16_t)RG_LIGHT_ARROWS) ? (uint8_t)ITEM_ARROW_LIGHT
                                                     : (uint8_t)ITEM_ARROW_FIRE;
}
static bool OoT_SharedArrowHeld(uint16_t nativeId) {
    const uint8_t item = OoT_SharedArrowItem(nativeId);
    return INV_CONTENT(item) == item;
}
static void OoT_SharedArrowGive(uint16_t nativeId) {
    const uint8_t item = OoT_SharedArrowItem(nativeId);
    INV_CONTENT(item) = item;
}

// #978: the Goron, Zora, Keaton and Truth masks and the Bunny Hood (OoTMM
// sharedMaskGoron, Zora, Bunny, Keaton, Truth). OoT owns a shuffled mask through
// its RAND_INF_CHILD_TRADES_HAS_MASK_* flag (every paired world shuffles the
// Happy Mask Shop's masks, #973), and its get-item also puts the mask in the
// child trade slot when that slot is empty (randomizer.cpp, the RG_KEATON_MASK
// ... RG_MASK_OF_TRUTH branch); the give does both, as that get-item does.
// OoT's Goron and Zora masks stay cosmetic trade masks: nothing here makes them
// transform.
static RandomizerInf OoT_SharedMaskFlag(uint16_t nativeId) {
    return (RandomizerInf)((int)RAND_INF_CHILD_TRADES_HAS_MASK_KEATON + ((int)nativeId - (int)RG_KEATON_MASK));
}
static bool OoT_SharedMaskHeld(uint16_t nativeId) {
    return Flags_GetRandomizerInf(OoT_SharedMaskFlag(nativeId)) != 0;
}
static void OoT_SharedMaskGive(uint16_t nativeId) {
    Flags_SetRandomizerInf(OoT_SharedMaskFlag(nativeId));
    if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
        INV_CONTENT(ITEM_TRADE_CHILD) = (u8)((int)ITEM_MASK_KEATON + ((int)nativeId - (int)RG_KEATON_MASK));
    }
}
// The arithmetic above is randomizer.cpp's own: the eight masks are contiguous,
// in the same order, in RandomizerGet, RandomizerInf and ItemID.
static_assert((int)RG_MASK_OF_TRUTH - (int)RG_KEATON_MASK == 7 &&
                  (int)RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH - (int)RAND_INF_CHILD_TRADES_HAS_MASK_KEATON == 7 &&
                  (int)ITEM_MASK_TRUTH - (int)ITEM_MASK_KEATON == 7 &&
                  (int)RG_GORON_MASK - (int)RG_KEATON_MASK == (int)ITEM_MASK_GORON - (int)ITEM_MASK_KEATON &&
                  (int)RAND_INF_CHILD_TRADES_HAS_MASK_ZORA - (int)RAND_INF_CHILD_TRADES_HAS_MASK_KEATON ==
                      (int)RG_ZORA_MASK - (int)RG_KEATON_MASK &&
                  (int)RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY - (int)RAND_INF_CHILD_TRADES_HAS_MASK_KEATON ==
                      (int)RG_BUNNY_HOOD - (int)RG_KEATON_MASK,
              "the masks' RG_*, RAND_INF_* and ITEM_* ids must stay contiguous and in the same order");

// #979: the Blast Mask OoT gains under the Blast Mask (OoT) rule (OoTMM
// blastMaskOot), OoT's copy of the Shared Blast Mask family (sharedMaskBlast,
// which requires it). OoT owns it through RAND_INF_OBTAINED_BLAST_MASK, and its
// get-item also puts it in an empty child trade slot (randomizer.cpp); the give
// does both, as the eight masks' give does.
static bool OoT_SharedBlastMaskHeld(uint16_t nativeId) {
    (void)nativeId;
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK) != 0;
}
static void OoT_SharedBlastMaskGive(uint16_t nativeId) {
    (void)nativeId;
    Flags_SetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
        INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_BLAST;
    }
}

// #979: the Stone Mask OoT gains under the Stone Mask (OoT) rule (OoTMM
// stoneMaskOot), OoT's copy of the Shared Stone Mask family (sharedMaskStone),
// held and given as the Blast Mask's is.
static bool OoT_SharedStoneMaskHeld(uint16_t nativeId) {
    (void)nativeId;
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_STONE_MASK) != 0;
}
static void OoT_SharedStoneMaskGive(uint16_t nativeId) {
    (void)nativeId;
    Flags_SetRandomizerInf(RAND_INF_OBTAINED_STONE_MASK);
    if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
        INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_STONE;
    }
}

// #1169 XW10: Ocarina of Time's six warp songs as the OoT members of the shared
// warp songs (OoTMM sharedSongMinuet .. sharedSongPrelude, the second family
// word). Item_Give(ITEM_SONG_MINUET ..) only sets the song's quest bit, and the
// ocarina staff builds its playable set from the quest bits alone (z_message_PAL.c,
// the CHECK_QUEST_ITEM(QUEST_SONG_MINUET + i) loop), as for Epona's Song above.
// Their own map, NOT OoT_SharedSongQuestBit: that one answers Epona's bit for
// every id but the Song of Storms, so a warp song there would read and give
// Epona's Song.
static uint32_t OoT_SharedWarpSongQuestBit(uint16_t nativeId) {
    if (nativeId < (uint16_t)RG_MINUET_OF_FOREST || nativeId > (uint16_t)RG_PRELUDE_OF_LIGHT) {
        return 0u;
    }
    return 1u << ((int)QUEST_SONG_MINUET + ((int)nativeId - (int)RG_MINUET_OF_FOREST));
}
static bool OoT_SharedWarpSongHeld(uint16_t nativeId) {
    const uint32_t bit = OoT_SharedWarpSongQuestBit(nativeId);
    return bit != 0u && (gSaveContext.inventory.questItems & bit) != 0u;
}
static void OoT_SharedWarpSongGive(uint16_t nativeId) {
    const uint32_t bit = OoT_SharedWarpSongQuestBit(nativeId);
    if (bit != 0u) {
        gSaveContext.inventory.questItems |= bit;
    }
}
static_assert((int)RG_PRELUDE_OF_LIGHT - (int)RG_MINUET_OF_FOREST == 5 &&
                  (int)QUEST_SONG_PRELUDE - (int)QUEST_SONG_MINUET == 5,
              "the six warp songs' RG_* and QUEST_SONG_* ids must stay contiguous and in the same order (#1169 XW10)");

// #837: defined below, beside OoT_SaveIsLiveFile (the harvest's gate).
extern "C" int OoT_Combo_DepartureIsLiveFile(void);

/** #1169 XW14: OoT's live probe for Combo_SharedFamilies2HeldNow: nonzero only while
 *  OoT is the running game and its gSaveContext is a live file (the harvest's own
 *  gameMode gate), so OoT's members are never asked about another game's save. */
static int OoT_SharedFamilyLiveProbe(void) {
    return (Context_GetCurrentGame() == GAME_OOT && OoT_Combo_DepartureIsLiveFile() != 0) ? 1 : 0;
}

namespace {
// Static-init registration, the ComboLogicEngine registrars' shape: this TU is
// always linked (it holds the shims), and the registry is zero-initialised
// storage, so the order against other static constructors does not matter. The
// bag reads it at the creation event and the shims at every crossing.
struct OoTSharedFamilyRegistrar {
    OoTSharedFamilyRegistrar() {
        Combo_RegisterSharedLiveProbe(GAME_OOT, OoT_SharedFamilyLiveProbe); // #1169 XW14
        const ComboSharedFamilyMember lens = { (uint16_t)RG_LENS_OF_TRUTH, OoT_SharedLensHeld, OoT_SharedLensGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_LENS, &lens);
        const ComboSharedFamilyMember epona = { (uint16_t)RG_EPONAS_SONG, OoT_SharedSongHeld, OoT_SharedSongGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_SONG_EPONA, &epona);
        const ComboSharedFamilyMember storms = { (uint16_t)RG_SONG_OF_STORMS, OoT_SharedSongHeld, OoT_SharedSongGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_SONG_STORMS, &storms);
        const ComboSharedFamilyMember songOfTime = { (uint16_t)RG_SONG_OF_TIME, OoT_SharedSongOfTimeHeld,
                                                     OoT_SharedSongOfTimeGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_SONG_TIME, &songOfTime);
        const ComboSharedFamilyMember fire = { (uint16_t)RG_FIRE_ARROWS, OoT_SharedArrowHeld, OoT_SharedArrowGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_ARROW_FIRE, &fire);
        const ComboSharedFamilyMember ice = { (uint16_t)RG_ICE_ARROWS, OoT_SharedArrowHeld, OoT_SharedArrowGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_ARROW_ICE, &ice);
        const ComboSharedFamilyMember light = { (uint16_t)RG_LIGHT_ARROWS, OoT_SharedArrowHeld, OoT_SharedArrowGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_ARROW_LIGHT, &light);
        const ComboSharedFamilyMember maskGoron = { (uint16_t)RG_GORON_MASK, OoT_SharedMaskHeld, OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_GORON, &maskGoron);
        const ComboSharedFamilyMember maskZora = { (uint16_t)RG_ZORA_MASK, OoT_SharedMaskHeld, OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_ZORA, &maskZora);
        const ComboSharedFamilyMember maskBunny = { (uint16_t)RG_BUNNY_HOOD, OoT_SharedMaskHeld, OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_BUNNY, &maskBunny);
        const ComboSharedFamilyMember maskKeaton = { (uint16_t)RG_KEATON_MASK, OoT_SharedMaskHeld, OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_KEATON, &maskKeaton);
        const ComboSharedFamilyMember maskTruth = { (uint16_t)RG_MASK_OF_TRUTH, OoT_SharedMaskHeld,
                                                    OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_TRUTH, &maskTruth);
        const ComboSharedFamilyMember maskBlast = { (uint16_t)RG_BLAST_MASK, OoT_SharedBlastMaskHeld,
                                                    OoT_SharedBlastMaskGive }; // #979
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_BLAST, &maskBlast);
        const ComboSharedFamilyMember maskStone = { (uint16_t)RG_STONE_MASK, OoT_SharedStoneMaskHeld,
                                                    OoT_SharedStoneMaskGive }; // #979
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_STONE, &maskStone);
        // #980: the Shared Gerudo, Skull and Spooky Masks (OoTMM sharedMaskGerudo,
        // sharedMaskSkull, sharedMaskSpooky): OoT's own three masks, owned and given
        // as the eight masks are, each paired with the copy Majora's Mask gains
        // under its mask's (MM) rule (MM's OoTMasksMM.cpp registers that half).
        const ComboSharedFamilyMember maskGerudo = { (uint16_t)RG_GERUDO_MASK, OoT_SharedMaskHeld, OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_GERUDO, &maskGerudo);
        const ComboSharedFamilyMember maskSkull = { (uint16_t)RG_SKULL_MASK, OoT_SharedMaskHeld, OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_SKULL, &maskSkull);
        const ComboSharedFamilyMember maskSpooky = { (uint16_t)RG_SPOOKY_MASK, OoT_SharedMaskHeld, OoT_SharedMaskGive };
        Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_SPOOKY, &maskSpooky);
        // #1169 XW10: the six warp songs, OoT's native copies, each the HOST half
        // of its shared song (the second family word; OoTMM replaces this copy with
        // the shared item and adds no MM copy). Always pooled when the song is a
        // pool item (pooled NULL); MM's half is WarpSongsMM.cpp's.
        static const uint32_t kSharedWarp[6] = { RSBS_XITEM2_SHARED_SONG_MINUET,   RSBS_XITEM2_SHARED_SONG_BOLERO,
                                                 RSBS_XITEM2_SHARED_SONG_SERENADE, RSBS_XITEM2_SHARED_SONG_REQUIEM,
                                                 RSBS_XITEM2_SHARED_SONG_NOCTURNE, RSBS_XITEM2_SHARED_SONG_PRELUDE };
        for (int i = 0; i < 6; i++) {
            const ComboSharedFamilyMember song = { (uint16_t)((int)RG_MINUET_OF_FOREST + i), OoT_SharedWarpSongHeld,
                                                   OoT_SharedWarpSongGive };
            Combo_RegisterSharedFamily2Member(GAME_OOT, kSharedWarp[i], &song);
        }
    }
};
const OoTSharedFamilyRegistrar gOoTSharedFamilyRegistrar;
} // namespace

// #969: THE ONE READER of the Bunny Hood's behaviour in Hyrule. Every OoT site
// that asks SoH's "Bunny Hood Effect" enhancement (CVAR_ENHANCEMENT
// "MMBunnyHood", a BunnyHoodMode) asks this instead. With the shared Bunny Hood
// FROZEN on (#978; operator ruling 2026-10-18, epic #975 decision 2) the hood
// behaves as that enhancement does when on, whatever its own setting: at least
// "Faster Run", Majora's Mask's speed boost; a stronger setting of the player's
// own ("Faster + Longer Jump") stands. Read, never stored: the enhancement's
// CVar keeps the player's value. OoT's logic gains no edge from it, as SoH's
// own enhancement never did.
extern "C" int OoT_BunnyHoodEffectMode(void) {
    const int own = CVarGetInteger(CVAR_ENHANCEMENT("MMBunnyHood"), BUNNY_HOOD_VANILLA);
    if (own == BUNNY_HOOD_VANILLA && Combo_ComboCrossItemRule(RSBS_XITEM_SHARED_BUNNY)) {
        return BUNNY_HOOD_FAST;
    }
    return own;
}

// OoT's hookshot ceiling: 0 none, 1 hookshot, 2 longshot.
#define OOT_MAX_HOOKSHOT_TIER 2u

// The hookshot as a tier. Both games keep it in ONE inventory byte, so the
// "progressive item" is really a small monotonic number — which is why this is
// a shared RESOURCE and not a shared item. Nothing crosses but the number: MM's
// ITEM_LONGSHOT is a different id that its give path repurposes into a Red
// Potion, so passing a raw item id over the boundary would hand MM a potion.
static uint16_t OoT_ReadHookshotTier(void) {
    const u8 held = INV_CONTENT(ITEM_HOOKSHOT);
    if (held == ITEM_LONGSHOT) {
        return 2u;
    }
    if (held == ITEM_HOOKSHOT) {
        return 1u;
    }
    return 0u;
}

/**
 * Author OoT's hookshot byte for `tier`, including the part a bare INV_CONTENT
 * write would miss.
 *
 * OoT's own longshot give does more than swap the inventory byte: it rewrites
 * every C-button (and both age-specific equip sets) that currently holds the
 * hookshot, because the button stores the ITEM id rather than the slot
 * (z_parameter.c's ITEM_LONGSHOT branch). Skip that and a player who had the
 * hookshot equipped keeps firing the SHORT one after the pool hands them a
 * longshot — the upgrade silently does nothing until they re-equip it.
 *
 * The icon reload that vanilla pairs with those writes is deliberately NOT
 * copied: it needs a live PlayState and this runs from Play_Init before there
 * is one. The arrival's own interface init loads the C-button icons after this
 * point, so the texture follows the id without it.
 */
static void OoT_WriteHookshotTier(uint16_t tier) {
    if (tier == 0u) {
        return; // monotonic: never take one away
    }
    const u8 item = (tier >= 2u) ? (u8)ITEM_LONGSHOT : (u8)ITEM_HOOKSHOT;
    INV_CONTENT(ITEM_HOOKSHOT) = item;

    u8* const buttonSets[] = {
        gSaveContext.equips.buttonItems,
        gSaveContext.adultEquips.buttonItems,
        gSaveContext.childEquips.buttonItems,
    };
    // From index 1, never 0. Index 0 is the B button; every vanilla loop that
    // matches a hookshot against buttonItems starts at 1 (the ITEM_LONGSHOT
    // give and the button-restriction passes alike), and B is not a slot the
    // hookshot is equippable to. It is also not inert storage: SoH's Bottle
    // Adventure restoration deliberately stages arbitrary save bytes THROUGH
    // buttonItems[0], so a byte there that happens to equal 0x0A or 0x0B is
    // data, not an equipped hookshot, and rewriting it would corrupt the value
    // that restoration exists to reproduce.
    for (size_t set = 0; set < ARRAY_COUNT(buttonSets); set++) {
        for (size_t i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (buttonSets[set][i] == ITEM_HOOKSHOT || buttonSets[set][i] == ITEM_LONGSHOT) {
                buttonSets[set][i] = item;
            }
        }
    }
}

// OoT's ocarina ceiling: 0 none, 1 Fairy Ocarina, 2 Ocarina of Time (#668).
#define OOT_MAX_OCARINA_TIER 2u

// The ocarina as a tier, the hookshot's shape a second time: both games keep
// their ocarina in ONE inventory byte, so "one shared instrument" is a small
// monotonic number and nothing but the number crosses. Passing a raw item id
// would be wrong in the MM direction anyway — MM's ITEM_OCARINA_OF_TIME is a
// different enumerator in a different id space (ADR 0002).
//
// SLOT_OCARINA holds ITEM_OCARINA_FAIRY or ITEM_OCARINA_TIME, and INV_CONTENT()
// indexes by item rather than by slot, so both reads below name the same byte.
static uint16_t OoT_ReadOcarinaTier(void) {
    const u8 held = INV_CONTENT(ITEM_OCARINA_FAIRY);
    if (held == ITEM_OCARINA_TIME) {
        return 2u;
    }
    if (held == ITEM_OCARINA_FAIRY) {
        return 1u;
    }
    return 0u;
}

/**
 * Author OoT's ocarina byte for `tier`, including the part a bare INV_CONTENT
 * write would miss.
 *
 * OoT's own Ocarina of Time give rewrites every C-button (and, under rando,
 * both age-specific equip sets) that currently holds the FAIRY ocarina, because
 * a button stores the ITEM id and not the slot (z_parameter.c's
 * ITEM_OCARINA_TIME branch). Skip that and a player who had the fairy ocarina
 * equipped keeps the old icon and the old id after the pool upgrades them — the
 * same silent no-op the hookshot's writer above exists to avoid.
 *
 * The age-set rewrite is done UNCONDITIONALLY here where vanilla gates it on
 * IS_RANDO && LINK_IS_CHILD/ADULT. Vanilla can assume the other age's set will
 * be re-authored when the player time-travels; a cross-game apply runs from
 * Play_Init on an arrival that may be at either age and may never travel again,
 * and leaving the other set holding ITEM_OCARINA_FAIRY would resurrect the
 * un-upgraded id on the next age change. Writing both is idempotent.
 *
 * Interface_LoadItemIcon1 is deliberately NOT called: it needs a live PlayState
 * and this runs before there is one. The arrival's own interface init loads the
 * C-button icons after this point, exactly as for the hookshot.
 */
static void OoT_WriteOcarinaTier(uint16_t tier) {
    if (tier == 0u) {
        return; // monotonic: never take one away
    }
    const u8 item = (tier >= 2u) ? (u8)ITEM_OCARINA_TIME : (u8)ITEM_OCARINA_FAIRY;
    if (INV_CONTENT(ITEM_OCARINA_FAIRY) == ITEM_NONE || item == (u8)ITEM_OCARINA_TIME) {
        INV_CONTENT(ITEM_OCARINA_FAIRY) = item;
    }
    if (item != (u8)ITEM_OCARINA_TIME) {
        return; // nothing equipped can be stale: tier 1 never replaces an id
    }

    u8* const buttonSets[] = {
        gSaveContext.equips.buttonItems,
        gSaveContext.adultEquips.buttonItems,
        gSaveContext.childEquips.buttonItems,
    };
    // From index 1, never 0 — the same rule and the same reason as the hookshot
    // writer above: index 0 is the B button, which is not a slot the ocarina is
    // equippable to and which SoH's Bottle Adventure restoration stages
    // arbitrary save bytes through.
    for (size_t set = 0; set < ARRAY_COUNT(buttonSets); set++) {
        for (size_t i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (buttonSets[set][i] == ITEM_OCARINA_FAIRY) {
                buttonSets[set][i] = item;
            }
        }
    }
}

// The Kokiri Sword as a shared 0/1 (#886): OoT's owned-equipment bit, which is
// all OoT's own give writes (z_parameter.c's ITEM_SWORD_KOKIRI..ITEM_SWORD_BGS
// branch). Nothing else crosses: OoT's Master Sword and Biggoron's Sword are
// OoT's alone, and MM's Razor or Gilded Sword arrives as this one bit.
#define OOT_MAX_KOKIRI_SWORD 1u

static uint16_t OoT_ReadKokiriSword(void) {
    return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI) ? 1u : 0u;
}

// The bottle count as a shared tier (#970): how many of OoT's four bottle slots
// hold a bottle, whatever it holds (Ruto's Letter and a Big Poe included: each
// is a bottle in the inventory). The shared count is OoT's 0..4, so this is
// also the apply's cap. Contents never cross.
#define OOT_MAX_BOTTLE_COUNT 4u

static uint16_t OoT_ReadBottleCount(void) {
    uint16_t count = 0u;
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_4; slot++) {
        if (gSaveContext.inventory.items[slot] != ITEM_NONE) {
            count++;
        }
    }
    return count;
}

/** Raise OoT's bottle count to @p count by putting an EMPTY bottle in the next
 *  empty slots, as OoT's own Item_Give(ITEM_BOTTLE) does; a filled slot is never
 *  touched. */
static void OoT_WriteBottleCount(uint16_t count) {
    uint16_t held = OoT_ReadBottleCount();
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_4 && held < count; slot++) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            gSaveContext.inventory.items[slot] = ITEM_BOTTLE;
            held++;
        }
    }
}

// ============================================================================
// The Shared Shields families (#1068; OoTMM sharedShields)
// ============================================================================
//
// OoT's copies of the rule's two pairs: the Hylian Shield (the family at
// RSBS_XITEM_SHARED_SHIELDS, paired with MM's Hero's Shield) and the Mirror
// Shield (the family at RSBS_XFAMILY_SLOT_MIRROR_SHIELD). OoT keeps its own
// shields: HELD is the owned-equipment bit OoT's own give writes (a shop's
// Hylian Shield included, so buying one in Hyrule gives Termina the Hero's
// Shield), and the GIVE writes that bit. A given shield is also WORN when the
// player's own choice allows it: the Hylian Shield when Link wears no shield,
// the Mirror Shield when an adult wears none or the Hylian Shield (as MM's
// Mirror Shield replaces its Hero's Shield). A child is never put in the Mirror
// Shield, and a worn Deku Shield is the player's choice and stays. The startup
// entrance runs this before Player init, which reads the equips.

extern "C" void OoT_Inventory_ChangeEquipment(s16 equipment, u16 value);

static bool OoT_SharedShieldHeld(uint16_t nativeId) {
    const int inv = (nativeId == (uint16_t)RG_MIRROR_SHIELD) ? EQUIP_INV_SHIELD_MIRROR : EQUIP_INV_SHIELD_HYLIAN;
    return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, inv) != 0;
}

static void OoT_SharedShieldGive(uint16_t nativeId) {
    const bool mirror = nativeId == (uint16_t)RG_MIRROR_SHIELD;
    gSaveContext.inventory.equipment |=
        (u16)OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, mirror ? EQUIP_INV_SHIELD_MIRROR : EQUIP_INV_SHIELD_HYLIAN);
    const s32 worn = CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    if (mirror) {
        if (LINK_IS_ADULT && (worn == EQUIP_VALUE_SHIELD_NONE || worn == EQUIP_VALUE_SHIELD_HYLIAN)) {
            OoT_Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
        }
    } else if (worn == EQUIP_VALUE_SHIELD_NONE) {
        OoT_Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_HYLIAN);
    }
}

/** Register OoT's copies of the Shared Shields families (#1068). Idempotent;
 *  the static registrar below runs it at start-up, and the locks re-run it
 *  after a row that cleared the registry. */
extern "C" void OoT_RegisterSharedShieldFamilies(void) {
    const ComboSharedFamilyMember hylian = { (uint16_t)RG_HYLIAN_SHIELD, OoT_SharedShieldHeld, OoT_SharedShieldGive };
    const ComboSharedFamilyMember mirror = { (uint16_t)RG_MIRROR_SHIELD, OoT_SharedShieldHeld, OoT_SharedShieldGive };
    Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_SHIELDS, &hylian);
    Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XFAMILY_SLOT_MIRROR_SHIELD, &mirror);
}

namespace {
struct OoTSharedShieldRegistrar {
    OoTSharedShieldRegistrar() {
        OoT_RegisterSharedShieldFamilies();
    }
};
const OoTSharedShieldRegistrar gOoTSharedShieldRegistrar;
} // namespace

// ============================================================================
// The Shared Ocarina Buttons families (#1067; OoTMM sharedOcarinaButtons)
// ============================================================================
//
// OoT's copies of the rule's five pairs: RG_OCARINA_A_BUTTON at the rule's own
// bit, the four C buttons at their extra slots. Ownership is the
// RAND_INF_HAS_OCARINA_* flag OoT's own give writes and its note gate reads
// (hook_handlers.cpp VB_HAVE_OCARINA_NOTE_*); the RG ids and the flags run in
// the same order (logic.cpp's item-to-flag table).
//
// HELD only when OoT's own frozen shuffle is on: a file without it is given all
// five at creation (savefile.cpp) and finds none, and a harvest that read those
// flags would hand Termina five buttons nobody found, which the solver never
// proved. The GIVE writes the flag directly (Flags_SetRandomizerInf would also
// fire the flag-set hooks, and the startup entrance runs before gameplay); with
// OoT's shuffle off the flag is already set and the write changes nothing.

namespace {

RandomizerInf OoT_SharedButtonFlag(uint16_t nativeId) {
    return (RandomizerInf)(RAND_INF_HAS_OCARINA_A + ((int)nativeId - (int)RG_OCARINA_A_BUTTON));
}

/** The seed's own button-shuffle option (the generating or loaded context). */
bool OoT_SharedButtonOptionOn(void) {
    const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    return ctx != nullptr && ctx->GetOption(RSK_SHUFFLE_OCARINA_BUTTONS).Get() != RO_GENERIC_OFF;
}

bool OoT_SharedButtonShuffleOn(void) {
    return IS_RANDO && OoT_SharedButtonOptionOn();
}

/** POOLED (the bag, at generation): OoT's buttons are items of its pool only
 *  under its own shuffle. Without it no pair is built, so a button can never be
 *  hosted at an OoT check whose give HELD above would not see. */
bool OoT_SharedButtonPooled(uint16_t nativeId) {
    (void)nativeId;
    return OoT_SharedButtonOptionOn();
}

bool OoT_SharedButtonHeld(uint16_t nativeId) {
    const int flag = (int)OoT_SharedButtonFlag(nativeId);
    return OoT_SharedButtonShuffleOn() && (gSaveContext.ship.randomizerInf[flag >> 4] & (1 << (flag & 0xF))) != 0;
}

void OoT_SharedButtonGive(uint16_t nativeId) {
    const int flag = (int)OoT_SharedButtonFlag(nativeId);
    gSaveContext.ship.randomizerInf[flag >> 4] |= (u16)(1 << (flag & 0xF));
}

} // namespace

/** Register OoT's copies of the Shared Ocarina Buttons families (#1067).
 *  Idempotent; the static registrar below runs it at start-up, and the locks
 *  re-run it after a row that cleared the registry. */
extern "C" void OoT_RegisterSharedOcarinaButtonFamilies(void) {
    static const struct {
        uint32_t slot;
        RandomizerGet button;
    } kButtons[] = {
        { RSBS_XITEM_SHARED_OCARINA_BUTTONS, RG_OCARINA_A_BUTTON },
        { RSBS_XFAMILY_SLOT_OCARINA_C_UP, RG_OCARINA_C_UP_BUTTON },
        { RSBS_XFAMILY_SLOT_OCARINA_C_DOWN, RG_OCARINA_C_DOWN_BUTTON },
        { RSBS_XFAMILY_SLOT_OCARINA_C_LEFT, RG_OCARINA_C_LEFT_BUTTON },
        { RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT, RG_OCARINA_C_RIGHT_BUTTON },
    };
    for (const auto& b : kButtons) {
        const ComboSharedFamilyMember member = { (uint16_t)b.button, OoT_SharedButtonHeld, OoT_SharedButtonGive,
                                                 OoT_SharedButtonPooled };
        Combo_RegisterSharedFamilyMember(GAME_OOT, b.slot, &member);
    }
}

namespace {
struct OoTSharedOcarinaButtonRegistrar {
    OoTSharedOcarinaButtonRegistrar() {
        OoT_RegisterSharedOcarinaButtonFamilies();
    }
};
const OoTSharedOcarinaButtonRegistrar gOoTSharedOcarinaButtonRegistrar;
} // namespace

// ============================================================================
// The Shared Swim family (#1369; operator ruling 2026-11-02)
// ============================================================================
//
// OoT's copy of the pair's one swim: the standalone RG_BRONZE_SCALE row the
// Shared Swim split puts in OoT's pool (item_pool.cpp). Ownership is
// RAND_INF_CAN_SWIM, the flag OoT's own give of that row writes
// (randomizerGetToRandInf) and the void-on-water gate reads (hook_handlers.cpp).
//
// HELD only while OoT's seed shuffles swim: a file without the shuffle is given
// the flag at creation (savefile.cpp) and found nothing, so it has nothing to
// share. Under the frozen rule OoT always shuffles swim (the paired creation
// shapes OoT's option on when only MM shuffles it, playthrough.cpp). The GIVE
// writes the flag directly, as the buttons' does. POOLED is the seed's own
// option, the row the split adds.

namespace {

bool OoT_SharedSwimOptionOn(void) {
    const std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    return ctx != nullptr && ctx->GetOption(RSK_SHUFFLE_SWIM).Get() != RO_GENERIC_OFF;
}

bool OoT_SharedSwimPooled(uint16_t nativeId) {
    (void)nativeId;
    return OoT_SharedSwimOptionOn();
}

bool OoT_SharedSwimHeld(uint16_t nativeId) {
    (void)nativeId;
    const int flag = (int)RAND_INF_CAN_SWIM;
    return IS_RANDO && OoT_SharedSwimOptionOn() &&
           (gSaveContext.ship.randomizerInf[flag >> 4] & (1 << (flag & 0xF))) != 0;
}

void OoT_SharedSwimGive(uint16_t nativeId) {
    (void)nativeId;
    const int flag = (int)RAND_INF_CAN_SWIM;
    gSaveContext.ship.randomizerInf[flag >> 4] |= (u16)(1 << (flag & 0xF));
}

} // namespace

/** Is the bronze tier its own pool row in this world (#1369)? The frozen Shared
 *  Swim rule with OoT's seed shuffling swim. Then OoT's pool holds a standalone
 *  RG_BRONZE_SCALE in place of Progressive Scale's extra bronze copy
 *  (item_pool.cpp), and Progressive Scale gives silver and gold only: its give
 *  (item.cpp) and the logic's crediting of it (logic.cpp) skip the swim flag, a
 *  scale's logic needs the swim flag too, and the bronze row credits the swim
 *  flag (Logic::ApplyItemEffect). Off (every world without the rule), all four
 *  are SoH's own. The rule is read from the frozen record only, so the answer is
 *  the world's, never a key's. */
extern "C" bool OoT_SharedSwimSplitsScale(void) {
    return Combo_ComboCrossItemRule(RSBS_XITEM_SHARED_SWIM) && OoT_SharedSwimOptionOn();
}

/** Register OoT's copy of the Shared Swim family (#1369). Idempotent; the static
 *  registrar below runs it at start-up, and the locks re-run it after a row that
 *  cleared the registry. */
extern "C" void OoT_RegisterSharedSwimFamily(void) {
    const ComboSharedFamilyMember swim = { (uint16_t)RG_BRONZE_SCALE, OoT_SharedSwimHeld, OoT_SharedSwimGive,
                                           OoT_SharedSwimPooled };
    Combo_RegisterSharedFamilyMember(GAME_OOT, RSBS_XITEM_SHARED_SWIM, &swim);
}

namespace {
struct OoTSharedSwimRegistrar {
    OoTSharedSwimRegistrar() {
        OoT_RegisterSharedSwimFamily();
    }
};
const OoTSharedSwimRegistrar gOoTSharedSwimRegistrar;
} // namespace

/**
 * Would GRANTING this row's item be handing the player a shuffled CHECK the
 * seed placed elsewhere? Then the pool may not hand it over.
 *
 * OoT_Item_Give opens with exactly this refusal for sticks and nuts
 * (z_parameter.c:1876-1885, "in case something got missed"): with
 * RSK_SHUFFLE_DEKU_STICK_BAG or RSK_SHUFFLE_DEKU_NUT_BAG on, neither is
 * obtainable until its bag check is found. That gate matters HERE and not for
 * the wallet or the quiver because MM's side is not neutral: a fresh MM save is
 * BORN at stick and nut tier 1 (the Sram default table), so plain monotonic
 * sharing would push tier 1 into OoT on the very first crossing of every seed.
 *
 * Bombchus are the same hazard wearing different clothes. When the seed shuffles
 * them (RSK_BOMBCHU_BAG is not NONE) the player is meant to hold none until the
 * check is found, and Bombchus.cpp keys shop eligibility on
 * INV_CONTENT(ITEM_BOMBCHU) — so materializing the item here would open the
 * bombchu shops too.
 *
 * ONLY THE GRANT IS SUPPRESSED, not the whole row. The count apply still runs,
 * clamped to this game's real (often zero) capacity, and that is deliberate:
 * the apply is what transfers WATERMARK OWNERSHIP to OoT. Skipping it leaves
 * the watermark owned by MM, and then the first OoT harvest after the player
 * legitimately finds the bag takes the "no watermark for this game" seed path,
 * records the live count as already-counted, and silently discards the stock
 * the bag just granted. Applying a zero costs nothing (the player has none) and
 * keeps the pool's arithmetic honest across the moment the gate opens.
 */
static bool OoT_SharedAmmoGrantBlocked(const OoTSharedAmmo* row) {
    if (!IS_RANDO || OTRGlobals::Instance == nullptr || OTRGlobals::Instance->gRandoContext == nullptr) {
        return false;
    }
    if (row->item == ITEM_STICK) {
        return Randomizer_GetSettingValue(RSK_SHUFFLE_DEKU_STICK_BAG) != 0 && CUR_UPG_VALUE(UPG_STICKS) == 0;
    }
    if (row->item == ITEM_NUT) {
        return Randomizer_GetSettingValue(RSK_SHUFFLE_DEKU_NUT_BAG) != 0 && CUR_UPG_VALUE(UPG_NUTS) == 0;
    }
    if (row->item == ITEM_BOMBCHU) {
        return Randomizer_GetSettingValue(RSK_BOMBCHU_BAG) != RO_BOMBCHU_BAG_NONE &&
               INV_CONTENT(ITEM_BOMBCHU) == ITEM_NONE;
    }
    return false;
}

/**
 * Apply one row's COUNT from the pool: the arrival's per-row count step, and the
 * re-apply a pickup credit runs (#1061, OoT_CreditSharedPickup).
 *
 * Runs even when the grant is blocked (see OoT_SharedAmmoGrantBlocked: skipping
 * it loses the player's stock the moment the gate opens), and then against a cap
 * of ZERO, so nothing materializes and the whole count stays in the pool. The
 * row's own capacity is not enough for that: sticks and nuts read capacity 0 at
 * tier 0, but OoT_BombchuCapacity is 50 under the Single Bombchu Bag option
 * whether or not the bag has been found (only the progressive option reads the
 * bag's level). Without the zero, a blocked apply under Single wrote a count
 * with no item in the slot, the bag's own give then ASSIGNED 20 over it
 * (SeedContext.cpp, HandleGetBombchuBag), and the next harvest took the
 * difference out of the pool.
 */
static void OoT_ApplySharedAmmoCount(const OoTSharedAmmo* row, bool grantBlocked) {
    uint16_t count = OoT_ReadAmmo(row->item);
    const uint16_t cap = grantBlocked ? 0u : OoT_AmmoCapacity(row);
    if (Combo_ApplySharedResource(GAME_OOT, row->countKind, cap, &count)) {
        // Bombchus have no tier to carry the item across, so a nonzero
        // shared count is what implies the item here.
        if (!grantBlocked && count > 0u) {
            OoT_EnsureInventoryItem(row->item);
        }
        AMMO(row->item) = (s8)count;
    }
}

static uint16_t OoT_ReadHealthQuarters(void) {
    const uint16_t pieces =
        (uint16_t)((gSaveContext.inventory.questItems & OOT_HEART_PIECE_MASK) >> OOT_HEART_PIECE_SHIFT);
    const uint16_t capacity = gSaveContext.healthCapacity < 0 ? 0u : (uint16_t)gSaveContext.healthCapacity;
    // The canonical quantity and its inverse both live in src/common so there
    // is exactly one copy of the arithmetic; see Combo_MakeHealthQuarters for
    // why hearts are one number rather than two shared fields.
    return Combo_MakeHealthQuarters(capacity, pieces);
}

// Magic meter level (0 none / 1 single / 2 double), derived from the two
// acquired FLAGS. Never from magicLevel: Sram_OpenSave zeroes magicLevel on
// every file load and the interface treats "acquired but level 0" as its
// meter-regrow trigger, so magicLevel is transiently 0 on exactly the frames a
// suspend or a mid-play save can land on.
static uint16_t OoT_ReadMagicLevel(void) {
    if (!gSaveContext.isMagicAcquired) {
        return 0u;
    }
    return gSaveContext.isDoubleMagicAcquired ? 2u : 1u;
}

// Current magic with the state machine's pending CREDITS settled in — the
// magic twin of the rupeeAccumulator fold, except nothing is written back:
// advancing the watermark to the settled value is enough, because the machine
// completing later brings the live field to exactly this number (delta zero),
// and on a switch the arrival path wipes the machine to IDLE and the apply
// re-authors the amount from the pool.
//
//   - STEP_CAPACITY / FILL: the true amount is parked in magicFillTarget and
//     `magic` itself can read 0 — the file-load meter-regrow window
//     (z_file_choose parks the saved amount there) and the tail of every
//     magic-upgrade give.
//   - ADD: a magic jar's credit is stepping toward magicTarget.
//   - The CONSUME_* family is deliberately NOT settled: a staged debit dies
//     with the machine on a switch (the spell never completed, so its cost is
//     refunded), and on a mid-play save the machine finishes normally and the
//     next harvest picks the decrement up as an ordinary spend.
//
// A game with no meter reports 0 regardless of the raw field, so a stale
// `magic` value can never enter the pool as phantom credit.
static uint16_t OoT_ReadSettledMagic(void) {
    const uint16_t level = OoT_ReadMagicLevel();
    if (level == 0u) {
        return 0u;
    }
    int32_t settled = gSaveContext.magic < 0 ? 0 : (int32_t)gSaveContext.magic;
    switch (gSaveContext.magicState) {
        case MAGIC_STATE_STEP_CAPACITY:
        case MAGIC_STATE_FILL:
            if ((int32_t)gSaveContext.magicFillTarget > settled) {
                settled = (int32_t)gSaveContext.magicFillTarget;
            }
            break;
        case MAGIC_STATE_ADD:
            if ((int32_t)gSaveContext.magicTarget > settled) {
                settled = (int32_t)gSaveContext.magicTarget;
            }
            break;
        default:
            break;
    }
    // The parked PRE-regrow idiom is state-machine-invisible: a file load, the
    // debug-save injection, and this file's own apply all leave the true
    // amount in magicFillTarget with magic == 0, magicLevel == 0 and the
    // machine IDLE — and it STAYS that way for the whole arrival fade, because
    // the regrow trigger is transition-gated. A harvest landing inside that
    // window (F10 is polled ungated every frame; the interval autosave's
    // OnSaveFile also harvests) would otherwise read 0 against a watermark
    // holding the applied amount and debit the pool by a full meter. The
    // signature is exact — acquired flags set (level != 0 above) with
    // magicLevel still 0 and magic still 0 — and every OoT path that produces
    // it authors magicFillTarget, so the park is trustworthy here.
    if (gSaveContext.magicLevel == 0 && gSaveContext.magic == 0 && (int32_t)gSaveContext.magicFillTarget > settled) {
        settled = (int32_t)gSaveContext.magicFillTarget;
    }
    const int32_t cap = (int32_t)level * MAGIC_NORMAL_METER;
    return (uint16_t)(settled > cap ? cap : settled);
}

// The harvest gate's game-mode contract (see Combo_SaveIsLiveFile). Asserted
// rather than assumed: these are OoT's own enumerators, and a renumber upstream
// would silently invert the gate — accepting the title screen and rejecting
// gameplay, which is precisely the bug the gate exists to close.
static_assert(GAMEMODE_NORMAL == RSBS_GAMEMODE_NORMAL, "OoT GAMEMODE_NORMAL must match RSBS_GAMEMODE_NORMAL");
static_assert(GAMEMODE_TITLE_SCREEN == RSBS_GAMEMODE_TITLE_SCREEN,
              "OoT GAMEMODE_TITLE_SCREEN must match RSBS_GAMEMODE_TITLE_SCREEN");
static_assert(GAMEMODE_FILE_SELECT == RSBS_GAMEMODE_FILE_SELECT,
              "OoT GAMEMODE_FILE_SELECT must match RSBS_GAMEMODE_FILE_SELECT");
static_assert(GAMEMODE_END_CREDITS == RSBS_GAMEMODE_END_CREDITS,
              "OoT GAMEMODE_END_CREDITS must match RSBS_GAMEMODE_END_CREDITS");

/**
 * Is OoT's live gSaveContext a real loaded file being played (#525 follow-up)?
 *
 * Thin adapter over the shared policy so the two games cannot drift; all of the
 * reasoning — including why fileNum is deliberately NOT consulted, even though
 * OoT's title save is the 0xFF sentinel — lives in Combo_SaveIsLiveFile.
 */
static bool OoT_SaveIsLiveFile(void) {
    return Combo_SaveIsLiveFile(GAME_OOT, (int32_t)gSaveContext.gameMode);
}

// #837: Combo_FlushLiveStateForFreeze notes this before every OoT freeze, so
// the crossing commit (Switch_CommitCrossing) never writes a title or
// file-select half into the player's slot. Same gate as the harvest.
extern "C" int OoT_Combo_DepartureIsLiveFile(void) {
    return OoT_SaveIsLiveFile() ? 1 : 0;
}

// #1351: OoT's Z-targeting mode (0 Switch, 1 Hold) for Majora's Mask to mirror
// (MM_Combo_MirrorOoTZTargetSetting). SoH keeps it in global.sav and in
// gSaveContext, so OoT's frozen half carries it. Read from that FROZEN half,
// never the live gSaveContext: the two games share that storage
// (src/common/unified_save.c), and it holds MM's layout whenever MM asks. -1
// when OoT has no frozen half (MM entered without leaving OoT: an MM-only boot,
// a ROM-free row), so MM keeps its own.
extern "C" int OoT_Combo_FrozenZTargetSetting(void) {
    if (!Context_HasFrozenState(GAME_OOT)) {
        return -1;
    }
    const SaveContext* oot = static_cast<const SaveContext*>(Context_GetOoTSaveContext());
    if (oot == nullptr) {
        return -1;
    }
    return oot->zTargetSetting != 0 ? 1 : 0;
}

/**
 * OoT's half of the pre-freeze revive (#664), the twin of
 * MM_Combo_ReviveDeadHealthForFreeze (#626 / PR #650).
 *
 * THE ROUTE. OoT's game-over is vanilla, not an enhancement: Link's health hits
 * 0, GameOver_Update runs the death states, and the kaleido chain
 * (z_kaleido_scope_PAL.c, pauseCtx->state 8 through 0x11) shows "GAME OVER",
 * the save prompt and "Continue playing?". Health stays at 0 for that whole
 * stretch; the only writer that lifts it is the prompt's continue leg
 * (state 0x11). F10 is polled ungated every frame from OoT's graph loop
 * (graph.c, Combo_CheckHotSwap), so a press anywhere in that stretch breaks the
 * loop and reaches Combo_FreezeActiveGameForHotSwap(GAME_OOT) with health 0.
 * Before this hook, Combo_FlushLiveStateForFreeze's GAME_OOT branch flushed
 * scene flags only: the frozen OoT half carried a dead bar, and
 * OoT_HarvestSharedResources (Game_Suspend, after the freeze) read it verbatim
 * into the shared CONSUMABLE bar, which MM's apply ASSIGNS on arrival -- where
 * only its one-heart floor stood between the player and a dead spawn. Once a
 * watermark exists (any earlier crossing), the harvest debits the bar to 0 and
 * MM arrives with one heart, not the three the continue prompt would have given.
 *
 * THE RULE, symmetric with MM's: revive rather than refuse, because F10 from
 * the game-over screen is a switch taken with Link dead, and a switch owes a
 * RESUMABLE half (the reasoning #625 and ADR 0009 decision 4b record for MM).
 * The revived value is OoT's own continue value, the exact expression of the
 * kaleido continue leg and of Sram_OpenSave (z_sram.c): STARTING_HEALTH, three
 * hearts, or the full capacity when the FullHealthSpawn enhancement is on. MM's
 * twin writes MM's own continue literal (0x30) for the same reason: the player
 * gets what the continue prompt would have given them. The heal accumulator is
 * cleared as the continue leg clears it. The magic-meter regrow the continue
 * leg also starts is deliberately NOT replicated, for the reason #625 records:
 * it is multi-frame and this departure has no frames left.
 *
 * A FAIRY REVIVE IN PROGRESS IS NOT THE CONTINUE CASE. Player's death
 * handler (z_player.c) spends the bottled fairy at the killing blow
 * (OoT_Inventory_ConsumeFairy -> gameOverCtx.state = GAMEOVER_REVIVE_START),
 * but the refill -- healthAccumulator = MAX_HEALTH, which the interface update
 * pours in 4 per frame and clamps at the capacity -- is written only after a
 * 60-frame countdown that starts later still. Health sits at 0 for over a
 * second with the bottle already gone. The continue value there would take the
 * fairy AND the refill it paid for; the player owns that heal exactly as a live
 * bar owns its pending accumulator. So when a live PlayState shows the
 * game-over machine in its GAMEOVER_REVIVE_* range, the revive gives what the
 * fairy would have: MAX_HEALTH clamped to the capacity. Once the refill has
 * lifted health above 0 the bar is alive and passes through untouched,
 * accumulator and all.
 *
 * GATED ON gameMode (OoT_SaveIsLiveFile, the harvest's own gate), never
 * fileNum: a cross-game session is pinned to the 0xFF sentinel, and a revive
 * that fired where the harvest would not -- the title screen's attract save --
 * would edit a save nobody is playing. Idempotent: a live bar is untouched.
 *
 * The CVar read is guarded on a live Ship::Context: the CVar bridge
 * dereferences the singleton unconditionally, and headless rows reach this
 * freeze seam without one. Production always has one.
 */
extern "C" void OoT_Combo_ReviveDeadHealthForFreeze(void) {
    if (!OoT_SaveIsLiveFile()) {
        return;
    }
    if (gSaveContext.health > 0) {
        return;
    }
    const PlayState* play = OoT_gPlayState;
    const bool fairySpent = play != NULL && play->gameOverCtx.state >= GAMEOVER_REVIVE_START &&
                            play->gameOverCtx.state <= GAMEOVER_REVIVE_FADE_OUT;
    if (fairySpent) {
        // The spent fairy's refill (MAX_HEALTH through the accumulator),
        // clamped as the interface update clamps it.
        gSaveContext.health = gSaveContext.healthCapacity < MAX_HEALTH ? gSaveContext.healthCapacity : MAX_HEALTH;
    } else {
        bool fullHealthSpawn = false;
        auto context = Ship::Context::GetInstance();
        if (context != nullptr && context->GetConsoleVariables() != nullptr) {
            fullHealthSpawn = CVarGetInteger(CVAR_ENHANCEMENT("FullHealthSpawn"), 0) != 0;
        }
        gSaveContext.health = fullHealthSpawn ? gSaveContext.healthCapacity : STARTING_HEALTH;
    }
    gSaveContext.healthAccumulator = 0;
    fprintf(stderr, "[OoT] pre-freeze: revived a dead health bar to %d (%s) before the departure freeze (#664)\n",
            (int)gSaveContext.health, fairySpent ? "the spent fairy's refill" : "the continue value");
    fflush(stderr);
}

// ============================================================================
// ONE KIND AT A TIME (#1120). The harvest and the apply are written per KIND,
// each kind with the quantity its ceiling bounds, so the arrival shims below
// and the pickup credit (OoT_CreditSharedPickup) run the SAME arithmetic: a
// far game's tier item found in Hyrule harvests its kind, steps the pool and
// re-applies its kind in place, which is exactly the arrival's block for that
// kind. The counted bombchu row has no tier and keeps its own #1061 path.
// ============================================================================

// The ammo row a capacity tier kind belongs to, or null for a kind that is
// not an ammo tier.
static const OoTSharedAmmo* OoT_SharedAmmoRowForTier(uint8_t kind) {
    for (int i = 0; i < ARRAY_COUNT(kOoTSharedAmmo); i++) {
        if (kOoTSharedAmmo[i].tierKind != RSBS_SHARED_RES_NONE && kOoTSharedAmmo[i].tierKind == kind) {
            return &kOoTSharedAmmo[i];
        }
    }
    return nullptr;
}

// The cap OoT's apply bounds a MONOTONIC kind with: the highest value OoT's
// own tables define, so a pool value authored by a future build with more
// tiers cannot index off the end of them (see each OOT_MAX_*). 0 for a kind
// this table does not bound (the triforce count derives its cap per world).
static uint16_t OoT_SharedKindCap(uint8_t kind) {
    switch (kind) {
        case RSBS_SHARED_RES_WALLET_TIER:
            return (uint16_t)OOT_MAX_WALLET_TIER;
        case RSBS_SHARED_RES_HEALTH_QUARTERS:
            return (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS;
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
            return 1u;
        case RSBS_SHARED_RES_MAGIC_LEVEL:
            return 2u;
        case RSBS_SHARED_RES_QUIVER_TIER:
        case RSBS_SHARED_RES_BOMB_BAG_TIER:
        case RSBS_SHARED_RES_STICK_TIER:
        case RSBS_SHARED_RES_NUT_TIER:
            return (uint16_t)OOT_MAX_AMMO_TIER;
        case RSBS_SHARED_RES_HOOKSHOT_TIER:
            return (uint16_t)OOT_MAX_HOOKSHOT_TIER;
        case RSBS_SHARED_RES_OCARINA_TIER:
            return (uint16_t)OOT_MAX_OCARINA_TIER;
        case RSBS_SHARED_RES_KOKIRI_SWORD:
            return (uint16_t)OOT_MAX_KOKIRI_SWORD;
        case RSBS_SHARED_RES_BOTTLE_COUNT:
            return (uint16_t)OOT_MAX_BOTTLE_COUNT;
        default:
            return 0u;
    }
}

// SETTLE THE ACCUMULATOR, then harvest the rupees. Both games write
// rupeeAccumulator, not the count, and drain it one per frame. A pending
// accumulator would otherwise ride the frozen blob and drain into OoT later —
// after an apply has already written an authoritative count — crediting the
// player twice. Fold it in and zero it so what we harvest is what OoT actually
// has. Clamped to the wallet, which is why the wallet tier's harvest runs this.
static void OoT_HarvestSharedRupees(void) {
    const int32_t walletCap = (int32_t)CUR_CAPACITY(UPG_WALLET);
    int32_t liveRupees = (int32_t)gSaveContext.rupees + (int32_t)gSaveContext.rupeeAccumulator;
    if (liveRupees < 0) {
        liveRupees = 0;
    }
    if (liveRupees > walletCap) {
        liveRupees = walletCap;
    }
    gSaveContext.rupees = (s16)liveRupees;
    gSaveContext.rupeeAccumulator = 0;
    Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, (uint16_t)liveRupees);
}

/**
 * HARVEST one MONOTONIC kind from OoT's live save, and with it the quantity its
 * ceiling bounds (rupees under the wallet, current health under the capacity,
 * current magic under the level, the count under an ammo tier): the harvest
 * shim's block for that kind, and step 1 of a pickup credit (#1120). Not gated
 * here: the shim gates on the live file before its first call, and the credit
 * shim likewise.
 */
static void OoT_HarvestSharedKind(uint8_t kind) {
    switch (kind) {
        case RSBS_SHARED_RES_WALLET_TIER:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, (uint16_t)CUR_UPG_VALUE(UPG_WALLET));
            OoT_HarvestSharedRupees();
            break;
        case RSBS_SHARED_RES_HEALTH_QUARTERS:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS, OoT_ReadHealthQuarters());
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT,
                                        gSaveContext.health < 0 ? 0u : (uint16_t)gSaveContext.health);
            break;
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE,
                                        gSaveContext.isDoubleDefenseAcquired ? 1u : 0u);
            break;
        case RSBS_SHARED_RES_MAGIC_LEVEL:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_LEVEL, OoT_ReadMagicLevel());
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, OoT_ReadSettledMagic());
            break;
        case RSBS_SHARED_RES_QUIVER_TIER:
        case RSBS_SHARED_RES_BOMB_BAG_TIER:
        case RSBS_SHARED_RES_STICK_TIER:
        case RSBS_SHARED_RES_NUT_TIER: {
            const OoTSharedAmmo* row = OoT_SharedAmmoRowForTier(kind);
            if (row != nullptr) {
                Combo_HarvestSharedResource(GAME_OOT, row->tierKind, (uint16_t)CUR_UPG_VALUE(row->upgrade));
                Combo_HarvestSharedResource(GAME_OOT, row->countKind, OoT_ReadAmmo(row->item));
            }
            break;
        }
        case RSBS_SHARED_RES_HOOKSHOT_TIER:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_HOOKSHOT_TIER, OoT_ReadHookshotTier());
            break;
        case RSBS_SHARED_RES_OCARINA_TIER:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, OoT_ReadOcarinaTier());
            break;
        case RSBS_SHARED_RES_KOKIRI_SWORD:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_KOKIRI_SWORD, OoT_ReadKokiriSword());
            break;
        case RSBS_SHARED_RES_BOTTLE_COUNT:
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_BOTTLE_COUNT, OoT_ReadBottleCount());
            break;
        case RSBS_SHARED_RES_TRIFORCE_PIECES:
            // OoT's counter is a MIRROR of the one combo count, so harvesting it
            // max-merges OoT's collects into that count.
            Combo_HarvestSharedResource(GAME_OOT, RSBS_SHARED_RES_TRIFORCE_PIECES,
                                        (uint16_t)gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected);
            break;
        default:
            break;
    }
}

/**
 * APPLY one ammo row from the pool: the tier BEFORE the count it bounds, for
 * the same reason wallet precedes rupees — a count applied against a capacity
 * of zero clamps to zero and the watermark records that zero as materialized,
 * quietly dropping the rest of the pool. The arrival's per-row step, and the
 * re-apply a tier credit runs (#1120).
 */
static void OoT_ApplySharedAmmoRow(const OoTSharedAmmo* row) {
    const bool grantBlocked = OoT_SharedAmmoGrantBlocked(row);

    if (!grantBlocked && row->tierKind != RSBS_SHARED_RES_NONE) {
        uint16_t tier = (uint16_t)CUR_UPG_VALUE(row->upgrade);
        if (Combo_ApplySharedResource(GAME_OOT, row->tierKind, OOT_MAX_AMMO_TIER, &tier)) {
            OoT_Inventory_ChangeUpgrade(row->upgrade, (s16)tier);
            if (tier >= 1u) {
                OoT_EnsureInventoryItem(row->item);
            }
        }
    }

    // Runs even when the grant is blocked, against a zero cap then (see
    // OoT_ApplySharedAmmoCount for both halves of that).
    OoT_ApplySharedAmmoCount(row, grantBlocked);
}

/**
 * APPLY one MONOTONIC kind from the pool into OoT's live save, and after it the
 * quantity its ceiling bounds: the arrival shim's block for that kind
 * (OoT_ApplySharedResources runs these in order), and step 3 of a pickup
 * credit (#1120), which re-applies exactly the kind it stepped.
 *
 * ORDER MATTERS inside each case. The wallet tier and the health capacity are
 * applied BEFORE the quantities they bound, because each quantity is clamped
 * to the ceiling the arriving game can actually display — and that ceiling is
 * what the lines above may have just raised.
 */
// True only while OoT_ApplySharedResourcesAtFileLoad runs (#1163). A file load
// parks current magic itself once the load is done (FileChoose_LoadGame copies
// `magic` into magicFillTarget after OoT_Sram_OpenSave), so the load's apply
// leaves the amount in `magic` for that idiom instead of parking it.
static bool sOoTApplyAtFileLoad = false;

static void OoT_ApplySharedKind(uint8_t kind) {
    switch (kind) {
        case RSBS_SHARED_RES_WALLET_TIER: {
            // --- Wallet tier (monotonic). Raises OoT's clamp before rupees land.
            uint16_t walletTier = (uint16_t)CUR_UPG_VALUE(UPG_WALLET);
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_WALLET_TIER, OOT_MAX_WALLET_TIER, &walletTier)) {
                OoT_Inventory_ChangeUpgrade(UPG_WALLET, (s16)walletTier);
            }
            // #991/#992 (operator ruling R2, 2026-10-19): OoT has ONE wallet
            // progression shared by child and adult, and a wallet found in Termina
            // IS the pair's wallet. Shuffle Child's Wallet shuffles the starting
            // wallet of that progression, not a second item, so a tier of 1 or more
            // implies the possession flag. Without it the tier rises with the flag
            // clear (a state SoH alone cannot reach): logic's GetWalletCapacity
            // reads the tier while the frame handler pins rupees at 0.
            if (IS_RANDO && walletTier >= 1) {
                Flags_SetRandomizerInf(RAND_INF_HAS_WALLET);
            }

            // --- Rupees (consumable), clamped to the wallet capacity just applied.
            uint16_t rupees = gSaveContext.rupees < 0 ? 0u : (uint16_t)gSaveContext.rupees;
            // With no wallet yet (Shuffle Child's Wallet on, flag clear, tier 0) OoT
            // holds no rupees: the frame handler pins the count at 0. Apply against
            // a ZERO capacity then, the way the ammo grant block does
            // (OoT_SharedAmmoGrantBlocked): the pool keeps its balance, OoT shows
            // 0, and the watermark moves to 0 so the next harvest reads no drop to
            // debit from the pool (#992).
            const uint16_t rupeeCap =
                (!IS_RANDO || Flags_GetRandomizerInf(RAND_INF_HAS_WALLET)) ? (uint16_t)CUR_CAPACITY(UPG_WALLET) : 0u;
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_RUPEES, rupeeCap, &rupees)) {
                gSaveContext.rupees = (s16)rupees;
            }
            // Whatever the restored blob had pending would drain on top of the
            // count we just authored. Zero it: the harvest that produced this pool
            // already folded OoT's accumulator in.
            gSaveContext.rupeeAccumulator = 0;
            break;
        }
        case RSBS_SHARED_RES_HEALTH_QUARTERS: {
            // --- Health capacity + pieces, from the ONE canonical quantity.
            uint16_t quarters = OoT_ReadHealthQuarters();
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_QUARTERS,
                                          (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS, &quarters)) {
                // Split back: whole hearts to capacity, the remainder to the piece
                // nibble. The 320 clamp inside the split is load-bearing — NEITHER
                // game's give path clamps capacity, and a total accumulated across
                // two pools of pieces and containers exceeds 20 hearts easily. The
                // life meter past 20 is untested in both ports.
                uint16_t capacity = 0;
                uint16_t pieces = 0;
                Combo_SplitHealthQuarters(quarters, &capacity, &pieces);
                gSaveContext.healthCapacity = (s16)capacity;
                gSaveContext.inventory.questItems = (gSaveContext.inventory.questItems & ~OOT_HEART_PIECE_MASK) |
                                                    ((uint32_t)pieces << OOT_HEART_PIECE_SHIFT);
            }

            // --- Current health (consumable). One bar across both games, per
            // OoTMM: "current health is tracked as if OoT and MM were one game with
            // a single health bar". Clamped to the capacity applied above.
            uint16_t health = gSaveContext.health < 0 ? 0u : (uint16_t)gSaveContext.health;
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_HEALTH_CURRENT,
                                          (uint16_t)gSaveContext.healthCapacity, &health)) {
                // Floor at one heart. A departing game cannot hand over a dead bar:
                // an F10 from either game's game-over screen revives before the
                // freeze (MM_Combo_ReviveDeadHealthForFreeze #626,
                // OoT_Combo_ReviveDeadHealthForFreeze #664), so this only fires on a
                // corrupt or hand-edited pool — and spawning dead on a cross-game
                // arrival lands in a death handler no arrival path has ever been
                // tested through.
                gSaveContext.health = (s16)(health < 0x10u ? 0x10u : health);
            }
            break;
        }
        case RSBS_SHARED_RES_DOUBLE_DEFENSE: {
            // --- Double defense (monotonic 0/1). Deliberately NOT a byte copy: the
            // flag is spelled isDoubleDefenseAcquired here and doubleDefense in MM,
            // and each game carries its own separate inventory.defenseHearts
            // counter that the life meter reads. Share the FACT, let each game set
            // its own pair.
            uint16_t doubleDefense = gSaveContext.isDoubleDefenseAcquired ? 1u : 0u;
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_DOUBLE_DEFENSE, 1u, &doubleDefense) &&
                doubleDefense != 0) {
                gSaveContext.isDoubleDefenseAcquired = 1;
                if (gSaveContext.inventory.defenseHearts < 20) {
                    gSaveContext.inventory.defenseHearts = 20;
                }
            }
            break;
        }
        case RSBS_SHARED_RES_MAGIC_LEVEL: {
            // --- Magic level (monotonic 0/1/2), then current magic clamped against
            // it. The settled-current snapshot is taken BEFORE the level apply moves
            // the acquired flags: the settled read gates on the game's own PRE-apply
            // level, which is what keeps a magic-less half's residue (see the MM
            // twin) from being promoted into real magic by the flag flip when the
            // pool carries a level but no current slot.
            uint16_t magic = OoT_ReadSettledMagic();
            uint16_t magicLevel = OoT_ReadMagicLevel();
            const bool magicLevelShared =
                Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_LEVEL, 2u, &magicLevel);
            if (magicLevelShared) {
                if (magicLevel >= 1u) {
                    gSaveContext.isMagicAcquired = 1;
                }
                if (magicLevel >= 2u) {
                    gSaveContext.isDoubleMagicAcquired = 1;
                }
            }

            // --- Current magic (consumable): one meter across both games, per
            // OoTMM: "current magic is tracked as if OoT and MM were one game with a
            // single magic meter". The cap is DERIVED from the level just applied,
            // never read from live magicCapacity — at this point in the arrival the
            // boot chain has not run the growth machine, so magicCapacity can be 0
            // or partial.
            const uint16_t magicCap = (uint16_t)(magicLevel * MAGIC_NORMAL_METER);
            const bool magicShared =
                Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_MAGIC_CURRENT, magicCap, &magic);

            if ((magicLevelShared || magicShared) && magicLevel != 0u && sOoTApplyAtFileLoad) {
                // A file load (#1163): the file select parks `magic` itself once
                // the load is done, so the amount goes where that idiom reads it.
                // Parked here it would be overwritten with the zero the idiom
                // finds in `magic`, and the next harvest would debit the pool by
                // the whole amount. The idiom then regrows the bar from the
                // acquired flags set above, as for any file load.
                gSaveContext.magic = (s16)(magic > magicCap ? magicCap : magic);
            } else if ((magicLevelShared || magicShared) && magicLevel != 0u) {
                // Re-author through the vanilla file-load idiom (z_file_choose):
                // park the amount in magicFillTarget, zero
                // magic/magicLevel/magicCapacity, idle the machine, and let
                // OoT_Interface_Update regrow the bar from the acquired flags
                // (STEP_CAPACITY -> FILL) exactly as every file load does — and as
                // OoT's own magic gives do mid-play (Item_Give zeroes magicLevel for
                // the same regrow). Direct field writes would have to reproduce the
                // level/capacity/HUD invariants by hand — and the idiom also repairs
                // a blob frozen mid-regrow, whose magicCapacity is otherwise
                // stranded at a partial value for the rest of the session (the
                // regrow trigger requires magicLevel == 0, which only this and a
                // real file load re-establish).
                gSaveContext.magicFillTarget = (s16)(magic > magicCap ? magicCap : magic);
                gSaveContext.magic = 0;
                gSaveContext.magicLevel = 0;
                gSaveContext.magicCapacity = 0;
                gSaveContext.magicState = MAGIC_STATE_IDLE;
                gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
            }
            break;
        }
        case RSBS_SHARED_RES_QUIVER_TIER:
        case RSBS_SHARED_RES_BOMB_BAG_TIER:
        case RSBS_SHARED_RES_STICK_TIER:
        case RSBS_SHARED_RES_NUT_TIER: {
            const OoTSharedAmmo* row = OoT_SharedAmmoRowForTier(kind);
            if (row != nullptr) {
                OoT_ApplySharedAmmoRow(row);
            }
            break;
        }
        case RSBS_SHARED_RES_HOOKSHOT_TIER: {
            // --- Hookshot (monotonic 0/1/2). Clamped to OoT's own ceiling of 2,
            // which is the higher of the two: MM has no longshot, so a longshot
            // earned here survives a Termina round trip untouched (max-merge cannot
            // demote it) and materializes there as a plain hookshot.
            uint16_t hookshotTier = OoT_ReadHookshotTier();
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_HOOKSHOT_TIER, OOT_MAX_HOOKSHOT_TIER,
                                          &hookshotTier)) {
                OoT_WriteHookshotTier(hookshotTier);
            }
            break;
        }
        case RSBS_SHARED_RES_OCARINA_TIER: {
            // --- Ocarina (monotonic 0/1/2), armed per world by the frozen combo
            // setting (#668; never armed since #879). Clamped to OoT's ceiling of
            // 2, the higher of the two: MM holds only one ocarina and harvests tier
            // 1, so an Ocarina of Time earned here survives a Termina round trip
            // untouched — max-merge cannot demote it — and MM's ocarina arrives as
            // the Fairy Ocarina rather than promoting the player to the Door of
            // Time's key. With the option off Combo_ApplySharedResource returns
            // false and nothing below runs.
            uint16_t ocarinaTier = OoT_ReadOcarinaTier();
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_OCARINA_TIER, OOT_MAX_OCARINA_TIER, &ocarinaTier)) {
                OoT_WriteOcarinaTier(ocarinaTier);
            }
            break;
        }
        case RSBS_SHARED_RES_KOKIRI_SWORD: {
            // --- The Kokiri Sword (monotonic 0/1), armed per world by the frozen
            // Shared Kokiri Sword setting (#886). Any sword MM holds arrives as the
            // Kokiri Sword's owned-equipment bit, exactly what OoT's own give
            // writes; the player equips it from the Equipment subscreen as after a
            // chest. Never taken away: the apply only raises. With the option off
            // Combo_ApplySharedResource returns false and nothing below runs.
            uint16_t kokiriSword = OoT_ReadKokiriSword();
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_KOKIRI_SWORD, OOT_MAX_KOKIRI_SWORD, &kokiriSword) &&
                kokiriSword >= 1u) {
                gSaveContext.inventory.equipment |= (u16)OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
            }
            break;
        }
        case RSBS_SHARED_RES_BOTTLE_COUNT: {
            // --- The bottle count (monotonic 0..4), armed per world by the frozen
            // Shared Bottles rule (#970). A bottle found in Termina is one more
            // EMPTY bottle here, in the next empty slot; what OoT's own bottles hold
            // stays OoT's. With the rule off Combo_ApplySharedResource returns false
            // and nothing below runs.
            uint16_t bottleCount = OoT_ReadBottleCount();
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_BOTTLE_COUNT, OOT_MAX_BOTTLE_COUNT, &bottleCount)) {
                OoT_WriteBottleCount(bottleCount);
            }
            break;
        }
        case RSBS_SHARED_RES_TRIFORCE_PIECES: {
            // --- Triforce pieces (monotonic, ADR 0010 answer O10), armed only when
            // the frozen combo goal is triforce-hunt. OoT's counter becomes the
            // whole combo count, so the next piece OoT gives adds to BOTH worlds'
            // collects and its give arm compares the combo requirement against it.
            // The cap is the combo total, which the creation rule bounds at OoT's
            // 8-bit counter; the 0xFF clamp restates that bound where the narrowing
            // happens.
            uint16_t triforcePieces = (uint16_t)gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
            const uint16_t triforceCap = Combo_TriforceHuntTotal() > 0xFFu ? 0xFFu : Combo_TriforceHuntTotal();
            if (Combo_ApplySharedResource(GAME_OOT, RSBS_SHARED_RES_TRIFORCE_PIECES, triforceCap, &triforcePieces)) {
                gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = (u8)triforcePieces;
            }
            break;
        }
        default:
            break;
    }
}

/**
 * HARVEST (#525). Fold OoT's live resource values into the shared pool.
 *
 * Called from OoT_Game_Suspend — the one point on BOTH the entrance and the F10
 * hot-swap path while gSaveContext still belongs to OoT — and immediately
 * before every `.redsave` write, so a file written mid-session carries a pool
 * that agrees with the OoT save stored beside it. Idempotent: a second call
 * with unchanged values is a no-op in both merge disciplines.
 *
 * GATED on a real loaded file. Without the gate, F10 on the title screen
 * harvests the ATTRACT DEMO's debug save — 14 hearts, single magic, and tier 1
 * of every ammo and wallet upgrade against a new file's zeroes — and because
 * those kinds are MONOTONIC they can never leave the pool again. See
 * Combo_SaveIsLiveFile.
 */
extern "C" void OoT_HarvestSharedResources(void) {
    if (!OoT_SaveIsLiveFile()) {
        // Early return BEFORE the accumulator settle and before the first
        // Combo_HarvestSharedResource call, so this touches neither the pool nor
        // the RAM watermark table: the next real harvest must find the
        // empty/occupied world its first-harvest seed rule expects. Skipping the
        // settle is right too — a menu's save has no balance worth settling, and
        // not writing gSaveContext leaves that save exactly as the menu left it.
        fprintf(stderr, "[OoT] shared-resource harvest skipped: not a live file (gameMode=%d fileNum=%d)\n",
                (int)gSaveContext.gameMode, (int)gSaveContext.fileNum);
        fflush(stderr);
        return;
    }

    OoT_HarvestSharedKind(RSBS_SHARED_RES_WALLET_TIER);     // and the rupees it bounds (settled first)
    OoT_HarvestSharedKind(RSBS_SHARED_RES_HEALTH_QUARTERS); // and the current health
    OoT_HarvestSharedKind(RSBS_SHARED_RES_DOUBLE_DEFENSE);
    OoT_HarvestSharedKind(RSBS_SHARED_RES_MAGIC_LEVEL); // and the current magic

    // Ammo: each tier and the count it bounds, then the tier-less bombchu count.
    // No settle step: unlike rupees and magic, neither game defers an ammo change
    // through an accumulator — every give and every shot is an immediate AMMO()
    // write with an inline clamp, so the live count is always already settled.
    for (int i = 0; i < ARRAY_COUNT(kOoTSharedAmmo); i++) {
        const OoTSharedAmmo* row = &kOoTSharedAmmo[i];
        if (row->tierKind != RSBS_SHARED_RES_NONE) {
            OoT_HarvestSharedKind(row->tierKind);
        } else {
            Combo_HarvestSharedResource(GAME_OOT, row->countKind, OoT_ReadAmmo(row->item));
        }
    }

    OoT_HarvestSharedKind(RSBS_SHARED_RES_HOOKSHOT_TIER);

    // Ocarina (#668), the Kokiri Sword (#886), the bottle count (#970) and the
    // triforce count (ADR 0010 O10): offered UNCONDITIONALLY from here. Each
    // arming gate lives in Combo_HarvestSharedResource, which consults the same
    // predicate the apply does, so a per-world option cannot end up gated on one
    // side only; with the option off the call touches neither the pool nor the
    // watermark table.
    OoT_HarvestSharedKind(RSBS_SHARED_RES_OCARINA_TIER);
    OoT_HarvestSharedKind(RSBS_SHARED_RES_KOKIRI_SWORD);
    OoT_HarvestSharedKind(RSBS_SHARED_RES_BOTTLE_COUNT);
    OoT_HarvestSharedKind(RSBS_SHARED_RES_TRIFORCE_PIECES);

    // Shared families (#976): OR in every armed family OoT's live save holds its
    // copy of. Behind the same live-file gate as everything above; a no-op until
    // a family is frozen on and registered (shared_resources.h).
    Combo_SharedFamiliesHarvestLive(GAME_OOT);
}

/**
 * FILL AT THE WALLET STEP (#1284; shared_resources.h): a wallet step that RAISED
 * OoT's tier, under the pair's frozen fill-wallets rule (OoT's Full Wallets),
 * fills OoT's rupees to the new cap the way OoT's own wallet give does under
 * that setting (z_parameter.c's Rupees_ChangeBy): through the accumulator, set
 * AFTER the apply, which zeroes it. The next harvest folds it into the pool,
 * clamped to the cap. A copy absorbed at the cap fills nothing.
 */
static void OoT_FillWalletAfterStep(s32 tierBefore) {
    if ((s32)CUR_UPG_VALUE(UPG_WALLET) <= tierBefore || !Combo_SharedWalletFillRule()) {
        return;
    }
    if (IS_RANDO && !Flags_GetRandomizerInf(RAND_INF_HAS_WALLET)) {
        return; // no wallet held: the frame handler pins rupees at 0 (#992)
    }
    const s32 fill = (s32)CUR_CAPACITY(UPG_WALLET) - (s32)gSaveContext.rupees;
    if (fill > 0) {
        gSaveContext.rupeeAccumulator = (s16)fill;
        fprintf(stderr, "[OoT] shared wallet step: fill-wallets rule on, %d rupees to the tier-%d cap of %d (#1284)\n",
                (int)fill, (int)CUR_UPG_VALUE(UPG_WALLET), (int)CUR_CAPACITY(UPG_WALLET));
    }
}

/**
 * STEP AT PICKUP (#1120; shared_resources.h): a far game's capacity tier found
 * in Hyrule (MM's Progressive Bomb Bag on an OoT check), or an OoT tier item
 * redeemed from an older save's crossing, steps the pool now and OoT's tier
 * re-applies from it at once: harvest the kind (and the quantity it bounds),
 * step the pool by one copy, re-apply the kind through the arrival's own block
 * for it (OoT_ApplySharedKind: the tier before the quantity, the UPG_* write,
 * the item slot, the ammo count from the pool). The harvest first is what keeps
 * everything found or spent since the arrival in the pool's arithmetic.
 *
 * @return 1 when stepped (including a copy absorbed at the cap: nothing in
 *         Termina could hold it either); 0 (the caller crosses the item, as
 *         before) when the kind is disarmed by the frozen rules or has no cap
 *         here, or (#1145) the item is MM's sword and the pool, after the
 *         harvest, already holds the Kokiri Sword: that copy is MM's Razor or
 *         Gilded Sword, which only MM's give can make.
 */
static int OoT_StepSharedTierAtPickup(const SharedItem* item, uint8_t kind, uint16_t units) {
    OoT_HarvestSharedKind(kind);
    if (!Combo_SharedPickupStepAdmitted(*item, kind)) {
        fprintf(stderr, "[OoT] shared pickup step: %s id=%u crosses, the pool already holds shared kind %u (#1145)\n",
                Game_ToString((GameId)item->originGame), (unsigned)item->id, (unsigned)kind);
        return 0;
    }
    const s32 walletTierBefore = (s32)CUR_UPG_VALUE(UPG_WALLET);
    if (!Combo_StepSharedTier(kind, units, OoT_SharedKindCap(kind))) {
        return 0; // the harvest alone changes nothing a later harvest would not
    }
    OoT_ApplySharedKind(kind);
    if (kind == RSBS_SHARED_RES_WALLET_TIER) {
        OoT_FillWalletAfterStep(walletTierBefore);
    }
    uint16_t pool = 0;
    Combo_GetSharedResource(kind, &pool);
    fprintf(stderr, "[OoT] shared pickup step: %s id=%u steps shared kind %u by %u; the pool now holds %u (#1120)\n",
            Game_ToString((GameId)item->originGame), (unsigned)item->id, (unsigned)kind, (unsigned)units,
            (unsigned)pool);
    return 1;
}

/**
 * CREDIT AT PICKUP (#1061; shared_resources.h): a counted pack found in Hyrule
 * (an MM bombchu pack on an OoT check), or an OoT pack redeemed from an older
 * save's crossing, goes into the pool now and OoT's count re-applies from it:
 * harvest OoT's bombchus, credit the pack, re-apply. The harvest first is what
 * keeps every bombchu spent since the arrival out of the re-applied count.
 * A capacity tier (#1120) takes the step path above instead.
 *
 * #1169 XW10: Majora's Mask's copy of an ARMED second-word shared family (a
 * shared warp song's surplus copy that crossed) is credited the same way: the
 * family is owned now and OoT is given its own copy, with no crossing
 * (Combo_SharedFamilies2CreditFarPickup). With the shared rule off (the extension
 * alone) it answers 0 and the item crosses as before.
 *
 * @return 1 when credited; 0 (nothing touched) when @p item is not a counted
 *         pack, a tier or an armed far family member, OoT's live save is not a
 *         real file, or the kind has no OoT row.
 */
extern "C" int OoT_CreditSharedPickup(const SharedItem* item) {
    uint8_t kind = 0;
    uint16_t units = 0;
    if (item != nullptr && OoT_SaveIsLiveFile() &&
        Combo_SharedFamilies2CreditFarPickup(GAME_OOT, item->originGame, item->id)) {
        return 1;
    }
    if (item == nullptr || !Combo_SharedPickupCreditOf(*item, &kind, &units) || !OoT_SaveIsLiveFile()) {
        return 0;
    }
    if (Combo_SharedResourceStepsAtPickup(kind)) {
        return OoT_StepSharedTierAtPickup(item, kind, units);
    }
    for (int i = 0; i < ARRAY_COUNT(kOoTSharedAmmo); i++) {
        const OoTSharedAmmo* row = &kOoTSharedAmmo[i];
        if (row->countKind != kind) {
            continue;
        }
        Combo_HarvestSharedResource(GAME_OOT, kind, OoT_ReadAmmo(row->item));
        if (!Combo_CreditSharedResource(kind, units)) {
            return 0; // the harvest alone changes nothing a later harvest would not
        }
        OoT_ApplySharedAmmoCount(row, OoT_SharedAmmoGrantBlocked(row));
        fprintf(stderr, "[OoT] shared pickup credit: %s id=%u adds %u to shared kind %u; OoT now holds %d\n",
                Game_ToString((GameId)item->originGame), (unsigned)item->id, (unsigned)units, (unsigned)kind,
                (int)AMMO(row->item));
        return 1;
    }
    return 0;
}

/**
 * APPLY (#525). Write the shared pool into OoT's live gSaveContext.
 *
 * Called from OoT_Play_Init's presence-gated startup-entrance branch beside
 * OoT_ConsumeSharedItems — the first point after the boot chain's last
 * gSaveContext wipe, and once per arrival into OoT. NOT Game_Resume: both
 * restores are re-authored afterwards by Opening_Init.
 *
 * ORDER MATTERS. Wallet tier and health capacity are applied BEFORE the
 * quantities they bound, because each quantity is clamped to the ceiling the
 * arriving game can actually display — and that ceiling is what the line
 * above may have just raised. Each kind's block, with the quantity it bounds,
 * is OoT_ApplySharedKind (#1120), which a pickup credit re-runs in place.
 */
extern "C" void OoT_ApplySharedResources(void) {
    OoT_ApplySharedKind(RSBS_SHARED_RES_WALLET_TIER);     // then the rupees it bounds
    OoT_ApplySharedKind(RSBS_SHARED_RES_HEALTH_QUARTERS); // then the current health
    OoT_ApplySharedKind(RSBS_SHARED_RES_DOUBLE_DEFENSE);
    OoT_ApplySharedKind(RSBS_SHARED_RES_MAGIC_LEVEL); // then the current magic

    // --- Ammo: every tier BEFORE the count it bounds, in one pass per row.
    for (int i = 0; i < ARRAY_COUNT(kOoTSharedAmmo); i++) {
        OoT_ApplySharedAmmoRow(&kOoTSharedAmmo[i]);
    }

    OoT_ApplySharedKind(RSBS_SHARED_RES_HOOKSHOT_TIER);
    OoT_ApplySharedKind(RSBS_SHARED_RES_OCARINA_TIER);
    OoT_ApplySharedKind(RSBS_SHARED_RES_KOKIRI_SWORD);
    OoT_ApplySharedKind(RSBS_SHARED_RES_BOTTLE_COUNT);
    OoT_ApplySharedKind(RSBS_SHARED_RES_TRIFORCE_PIECES);

    // Shared families (#976): OoT's own copy of every armed family found in
    // either game. Last, and still before the shared-item consumer (z_play.c).
    Combo_SharedFamiliesApplyLive(GAME_OOT);
}

/**
 * APPLY AT A FILE LOAD (#1163, #1159): the arrival's apply, run by OoT's .redsave
 * load seam (OoT_Combo_OnLoadFileSeam, SaveManager.cpp) so a loaded file starts
 * from the pool and its watermarks are set before anything can be earned or
 * spent. One difference from an arrival: current magic is written to `magic`,
 * not parked (see sOoTApplyAtFileLoad).
 */
extern "C" void OoT_ApplySharedResourcesAtFileLoad(void) {
    sOoTApplyAtFileLoad = true;
    OoT_ApplySharedResources();
    sOoTApplyAtFileLoad = false;
    // #1351: a file that already knows Epona's Song (Start with Epona's Song, or
    // a save written before this fix) gets its Epona here.
    OoT_ReconcileEponaObtained();
}

static bool sLastF10State = false;

/**
 * Check if F10 was pressed and request game switch.
 * Also checks for pending cross-game entrance switches.
 * Called from the OoT game loop (graph.c) each frame.
 */
extern "C" bool Combo_CheckHotSwap(void) {
    // Check for pending cross-game entrance switch first
    if (Combo_IsCrossGameSwitch()) {
        return true;
    }

    auto context = Ship::Context::GetInstance();
    if (!context) {
        return Combo_IsGameSwitchRequested();
    }

    auto window = context->GetWindow();
    if (!window) {
        return Combo_IsGameSwitchRequested();
    }

    int32_t scancode = window->GetLastScancode();
    bool f10Pressed = (scancode == Ship::LUS_KB_F10);

    // Refused here, while the running game is still in its loop, when the
    // other game cannot run (#954): see Switch_RefuseUnplayableTarget.
    if (f10Pressed && !sLastF10State) {
        Switch_RequestHotSwap();
    }
    sLastF10State = f10Pressed;

    return Combo_IsGameSwitchRequested();
}

/**
 * Check if an entrance triggers a cross-game switch.
 *
 * Dispatches against the currently active game so both OoT→MM and MM→OoT
 * work (issue #170). In single-exe mode this symbol is shared between both
 * games' translation units; the MM code path calls into this exact function
 * via the `Combo_CheckEntranceSwitch` extern, so we must freeze the right
 * side's SaveContext depending on who's running.
 *
 * Called from OoT's z_play.c / randomizer_entrance.c and from MM's
 * z_play.c / z_player.c.
 *
 * Returns the entrance the caller loads: `entranceIndex` itself, unless it is
 * a crossing into a game this session cannot run (#954). That crossing is
 * refused HERE, before anything is frozen and while the running game is still
 * in its frame loop, and the return is the running game's entrance just
 * outside the door (the link's return entrance: 0x01D1 outside the Happy Mask
 * Shop, 0xD800 outside the Clock Tower), so the transition already under way
 * puts Link back there. Refusing later was the bug: OoT's loop exited with the
 * switch pending, the launcher refused it and ran OoT's Main() again over the
 * retired gamestate, and the first frame read freed memory.
 */
extern "C" uint16_t Combo_CheckEntranceSwitch(uint16_t entranceIndex) {
    // Capture the pending state up front. If a cross-game switch is already
    // queued when we're called (e.g. a subsequent transition frame after the
    // initial trigger), we still let Combo_CheckCrossGameEntrance run so the
    // return value and any pending-switch updates match the pre-#170 contract
    // exactly — but we suppress the freeze + signal step below, so we don't
    // capture mutated state from a death sequence or mid-transition save.
    // The main loop will process the queued switch on the next iteration.
    bool wasAlreadyPending = Combo_IsCrossGameSwitch();

    // Resolve which game is running so we pick the correct entrance table
    // and freeze the correct SaveContext interpretation. Default to OoT when
    // the current-game tracker hasn't been populated yet (early boot).
    GameId currentGame = Context_GetCurrentGame();
    const char* gameId = (currentGame == GAME_MM) ? "mm" : "oot";

    uint16_t result = Combo_CheckCrossGameEntrance(gameId, entranceIndex);

    if (Combo_IsCrossGameSwitch() && !wasAlreadyPending) {
        const GameId target = Entrance_GetSwitchTargetGame();
        if (Switch_RefuseUnplayableTarget(target)) {
            const uint16_t outside = Combo_GetSwitchReturnEntrance();
            Combo_ClearPendingSwitch();
            fprintf(stderr,
                    "[COMBO] Crossing (%s) to %s refused at entrance 0x%04X: %s keeps running, Link goes back "
                    "outside the door (entrance 0x%04X)\n",
                    gameId, Game_ToString(target), entranceIndex, gameId, outside);
            return outside;
        }

        fprintf(stderr, "[COMBO] Cross-game switch (%s)! entrance=0x%04X\n", gameId, entranceIndex);

        // Publish the unified slot while OoT still knows it. This is the last
        // moment it is knowable: MM boots with gSaveContext.fileNum pinned to
        // the 0xFF sentinel (ConsoleLogo_Init) for the whole cross-game
        // session, and the only writers of a real 0..2 slot live in MM's own
        // file select, which a portal arrival never enters. Without this, an
        // MM-side save has no slot to address.
        //
        // Guarded two ways. Only when OoT is the DEPARTING game: sizeof and
        // field offsets in this TU are OoT's SaveContext layout, so reading
        // fileNum while MM is active would pull an unrelated MM field. And
        // only for an in-range value, so OoT's own 0xFF title-screen sentinel
        // does not clear a slot that a real load already established.
        if (currentGame != GAME_MM) {
            const int ootFileNum = static_cast<int>(gSaveContext.fileNum);
            if (ootFileNum >= 0 && ootFileNum < RSBS_SAVE_MAX_SLOTS) {
                RsbsSave_SetActiveSlot(ootFileNum);
            }
        }

        uint16_t returnEntrance = Combo_GetSwitchReturnEntrance();
        // Pre-freeze discipline (#638): this hook runs the instant nextEntrance
        // is assigned, before the scene transition that would normally copy the
        // live scene flags into gSaveContext (Actor_CleanupContext), and the
        // switch then stops the gamestate without ever running Play_Destroy.
        // Flush them now, or every flag set during the final scene visit is
        // frozen as unset and the pickup respawns on the return leg (#635).
        // Resolved the same way gameId is above: a not-yet-populated tracker
        // means OoT, never a third game.
        Combo_FlushLiveStateForFreeze((currentGame == GAME_MM) ? GAME_MM : GAME_OOT);
        // sizeof(gSaveContext) in this TU is OoT's SaveContext layout. When MM
        // is the active game this over-reads relative to MM's smaller struct,
        // but the underlying unified storage (unified_save.c) is
        // OOT_SAVE_CONTEXT_SIZE for both games, so the read stays in bounds
        // and Context_FreezeState clamps to the per-game blob capacity.
        Combo_FreezeState(gameId, returnEntrance, &gSaveContext, sizeof(gSaveContext));
        Combo_SignalReadyToSwitch();
    }

    return result;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
