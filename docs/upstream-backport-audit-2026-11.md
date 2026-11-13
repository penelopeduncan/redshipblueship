# Upstream backport delta audit (2026-11)

Follow-up delta audit to [`upstream-backport-audit-2026-10.md`](upstream-backport-audit-2026-10.md) against `origin/main` at `a28d658a4` (`a28d658a40eb53e0a5e2281a5f080f260319b4a9`). Verbatim import/cutoff baseline from the 2026-10 audit:

> Import bases: Ship of Harkinian e149e09e0 (2026-01-11, audited to dd702b067), 2Ship2Harkinian d05565623 (2026-01-10, audited to f1dda4c28), libultraship fork merge-base 8c55f607 (2026-01-11, audited to da6daf1a on port-maintenance); the doc applies to main at 7c1aecf9 (2026-10-26).

This document is read-only reconnaissance prepared directly from the local workspace and read-only upstream clones (`Shipwright.git`, `2ship2harkinian.git`, and `libultraship.git`). Nothing has been reported or filed upstream (HarbourMasters, Kenix3, or any external repository), and no issues have been filed here.

**In-flight RSBS backport PRs at `a28d658a4`:** `#1555` and `#1556` have merged; `#1558` (Wonder Items, `#1405`), `#1562` (Icicles / Red Ice, `#1408`), and `#1564` (Rocks / Boulders, `#1409`) remain open in flight and were checked for overlap.

## Scope, cutoffs, and xref boundary

Upstream merge commits were unpacked to individual non-merge commits so fixes folded into batch merges (notably SoH `#7423`, `#7427`, `#7431`, `#7479`; 2Ship2Harkinian `#2020`; libultraship `#1228`) are audited individually with nothing left opaque.

| Repository | Audited range | Tip timestamp | First-parent merges | Unpacked non-merge commits | Notes |
|---|---|---|---|---|---|
| Ship of Harkinian (`develop`) | `dd702b067..d4bf6528a` | `2026-11-01 20:58:28 +0000` | 5 (`aa89cc57`, `d458af71`, `41ae6cd3`, `2827fc2a`, `d4bf6528`) | 38 | `33` `<= 2827fc2a0` (pre-xref); `5` in `#7479` (post-xref) |
| 2Ship2Harkinian (`develop`) | `f1dda4c28..fd9fb03bd` | `2026-11-02 17:57:36 -0600` | 15 (`14` single-commit PRs + `#2020` `67cc3796`) | 21 | `5` first-parent `<= b30a286c5` (pre-xref); `16` unpacked post-xref |
| `libultraship` (`port-maintenance`) | `da6daf1a..57e6d409` | `2026-11-03 07:59:06 -0500` | 1 (`57e6d409` `#1228`) | 1 | `22aec8f3` merged post-xref; `.gitmodules` points to `penelopeduncan/libultraship.git` pinned at `05ae63384` |
| **Total** | | | **21** | **60** | |

Per [`upstream-issue-xref-2026-10-26.md`](upstream-issue-xref-2026-10-26.md#L5-L6), the xref read stopped at SoH `2827fc2a0` (2026-10-20), 2Ship2Harkinian `b30a286c5` (2026-10-24), and `libultraship` `da6daf1a`. Every commit in this delta is tagged `pre-xref` or `post-xref` relative to those boundaries.

## Summary counts by class

Every unpacked non-merge commit is classified exactly once into mutually exclusive classes matching [`upstream-backport-audit-2026-10.md`](upstream-backport-audit-2026-10.md):

| Class | Ship of Harkinian | 2Ship2Harkinian | `libultraship` | Total |
|---|---:|---:|---:|---:|
| Bundle for backport | 5 | 4 | 1 | 10 |
| Needs a ruling | 6 | 10 | 0 | 16 |
| Parked (`#1418` / `#1419`) | 1 | 0 | 0 | 1 |
| Not applicable to single-exe | 15 | 5 | 0 | 20 |
| Already here | 11 | 2 | 0 | 13 |
| **Total** | **38** | **21** | **1** | **60** |

## Changes to earlier bundles (`2026-10` cross-check)

These commits touch, supersede, or partially land earlier bundles in [`upstream-backport-audit-2026-10.md`](upstream-backport-audit-2026-10.md):

- **`L1` (`67861a90` `#1042` / `3097debf` `#1052` / `bbb565bd` `#1176`):** `libultraship` `22aec8f3` (merge `57e6d409`, `#1228`) widens Fast3D 64-bit resource pointer masking in `src/fast/interpreter.cpp:4795-4813` (`(i & 0x00FFFFFFFFFFFFFFull)`) so ARM64 tagged user-space pointers resolve instead of trapping. Fork-repo (`penelopeduncan/libultraship`) change; add to `L1`.
- **`O13` (`SaveManager` & gameplay stats):** SoH `4451710b` (`#7434`) adds defensive `.value(...)` / `sohStats` JSON checks when loading older saves (`games/oot/soh/SaveManager.cpp:1339,1376,1391`) — bundle-ready; SoH `8ccdbb98` (`#7385`) adds `gameComplete` to `gameplaystats.cpp` JSON save/load (`Needs a ruling`).
- **`O21` (small randomizer cleanups):** SoH `4a1b846c` (`#7357`) fixes remaining `"Southeast Boulder Super"` / `"Notheast Boulder Silver"` typo strings in `ShuffleSilver.cpp:335-336` alongside Fire Trial silver rupee (`430f7cde` `#7328`, already landed in `#1512`), `en_US.json` trick text, and comments — add to `O21`.
- **`O28` & `#1493` (OoT region/logic fixes):** SoH `28ab1206` (`#7396`) pairs an MQ Light Trial dead-exit cleanup already in RSBS (`games/oot/soh/Enhancements/randomizer/location_access/dungeons/ganons_castle.cpp:727-743`) with an MQ Fire Trial fix (`RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED`) blocked on the unported `O28` `ganons_castle.cpp` region split (`:494-505`). SoH `156c2a4a` (`#7417`) already had its `hyrule_field.cpp:17-64` grass `SummonEpona()` hunk landed by `#1493` (`d2504828b`); its remaining hunks require ruling (and overlap `#1419` Shuffle Signs and `#1564` Rocks).
- **`O30` (win condition / Ganon warp / `gameplaystats.h`):** SoH `c724b412` (`#7341`) and `9ffa4c86` (`#7404`) touch win-condition options, altar hints, `GID_TRIFORCE`, `BossDefeatTimestamps.cpp`, `gameplaystats.h`, and Ganon defeat castle-warp override; both move into `Needs a ruling`.
- **`O35` (QoL / debuggers):** SoH `5c59f847` (`#7383`) fixes `debugSaveEditor.cpp` player-name encoding (`SohGui.cpp:177-179` still exposes the debug save editor in single-exe).
- **`O36` (vanilla/rando boundary + hook hygiene):** SoH `f93e3cfb` (`#7415`) fixes `gRandomizerFlag` magic / Barinade soul hookification; `41ae6cd3` (`#7422`) moves `GameInteractor::RemoveAllQueuedHooks` out of header into `.cpp`.
- **`M3` (MM logic fixes):** 2Ship2Harkinian `b43af818` (`#2006`) already had its `PiratesFortress.cpp` fix landed via PR `#1516` (`a46e8f212`); the other 7 files plus `5fbb86ab` (`#1954`), `e4061b6f` (`#1984`), `224ec437` (`#2017`), and `a9a6009a` (`#2018`) await world-moving rulings.
- **`M12` (MM enhancements & `GreatFairySwordBButton`):** 2Ship2Harkinian `eb89f12b` (`#1870`, `Needs a ruling`) supersedes `GreatFairySwordBButton.cpp` with `gEnhancements.Equipment.ActiveItemOnB` and adds `ConfigVersion2Updater`; `6080354c` (`#2010`, bundleable hook prerequisite) refactors `TingleAlwaysInClockTown` (still unlinked/deferred in RSBS) and adds `OnActorEntryHalfDaysBits` needed by `a0e14e87` (`#1972`).
- **`M19` (build hygiene):** 2Ship2Harkinian `d58abb6f` (`#1814`) strips compiler warnings in compiled MM TUs; `M20` is opened below for MM text mod support (`faa1e13b` `#2000` + follow-up cleanup `222acd18` `#2013`).

## Bundle for backport (10 commits)

None of the items in this class add player-facing options, move generated rando worlds/goldens, or alter serialized save layouts.

### `libultraship` (`port-maintenance`) — 1 commit

| Bundle | Commit | Upstream PR | Xref | Priority | Effort | Suggested Tier | Summary & RSBS target |
|---|---|---|---|---|---|---|---|
| `L1` | `22aec8f3` (merge `57e6d409`) | `#1228` | post-xref | `P2` (`P0` on macOS/Android ARM64 with TBI/MTE) | `S` | `T3` | `fix(fast): accept tagged arm64 resource pointers`. Masks `(i & 0x00FFFFFFFFFFFFFFull)` in `src/fast/interpreter.cpp:4795-4813`. Lands in fork PR against `penelopeduncan/libultraship` (`05ae63384`), then submodule pin bump in RSBS. |

### Ship of Harkinian — 5 commits

| Bundle | Commit | Upstream PR | Xref | Priority | Effort | Suggested Tier | Summary & RSBS target |
|---|---|---|---|---|---|---|---|
| `O21` | `4a1b846c` | `#7357` | pre-xref (`#7423`) | `P3` | `S` | `T3` | `Fix typos in randomizer`. Fixes `"Southeast Boulder Super"` / `"Notheast Boulder Silver"` in `games/oot/soh/Enhancements/randomizer/ShuffleSilver.cpp:335-336`, `en_US.json:747,975` trick text, and comments in 9 logic/rando files. |
| `O35` | `5c59f847` | `#7383` | pre-xref (`#7423`) | `P3` | `S` | `T3` | `[Bugfix] Use game encoding for name in save editor`. Fixes PAL/NTSC character decode/encode in `games/oot/soh/Enhancements/debugger/debugSaveEditor.cpp` (live window registered in `SohGui.cpp:177-179`). |
| `O36` | `f93e3cfb` | `#7415` | pre-xref (`#7423`) | `P2` | `S` | `T3` | `[Bugfix] Randomizer flag can set vanilla infinite magic`. Separates vanilla/rando infinite-magic checks via `GIVanillaBehavior` + `hook_handlers.cpp` and hooks Barinade soul drop cleanly (`z_parameter.c`, `z_boss_va.c`). Keep single-exe `OoT_` prefixes if any touched symbol uses them. |
| `O36` | `41ae6cd3` | `#7422` | pre-xref | `P3` | `S` | `T3` | `move GameInteractor::RemoveAllQueuedHooks body to cpp`. Moves template/inline body from `GameInteractor.h` to `GameInteractor.cpp` to stop header bloat / ODR noise. |
| `O13` | `4451710b` | `#7434` | post-xref (`#7479`) | `P3` | `S` | `T3` | `[Bugfix] Older saves cannot be read`. Adds defensive `.value(...)` and `sohStats` guards in `games/oot/soh/SaveManager.cpp` (`:1339,1376,1391`). Preserve single-exe `.redsave` atomic temp-write flow (`:1334-1337`) and `rsbs/` save adapters. |

### 2Ship2Harkinian — 4 commits (all post-xref)

| Bundle | Commit | Upstream PR | Xref | Priority | Effort | Suggested Tier | Summary & RSBS target |
|---|---|---|---|---|---|---|---|
| `M20` | `faa1e13b` | `#2000` | post-xref | `P3` | `M` | `T3` | `Add text mod support`. Adds custom OTR/O2R MM text replacement in `games/mm/2s2h/resource/importer/TextMMFactory.{h,cpp}`, `resource/type/TextMM.h`, and `z_message_OTR.cpp` (compiled in `games/mm/CMakeLists.txt:798,831`). Skip the 2-line excluded `BenPort.cpp` hunk (`OTRGlobals`/init stays hosted in `rsbs/` if mounted). |
| `M20` | `222acd18` | `#2013` | post-xref | `P3` | `S` | `T3` | `Inline single-use ReadEntry and ReadBox`. Simplifies `TextMMFactory.cpp` right on top of `faa1e13b`; bundle together with `#2000`. |
| `M19` | `6080354c` | `#2010` | post-xref | `P3` | `S` | `T3` | `Add OnActorEntryHalfDaysBits hook and refactor TingleAlwaysInClockTown`. Adds `OnActorEntryHalfDaysBits` to `GameInteractor{.h,.cpp,_HookTable.h}` and `games/mm/src/code/z_actor.c:3658,4038` (prerequisite if `#1972` `a0e14e87` is approved). Note `TingleAlwaysInClockTown.cpp` remains unlinked/deferred in `M12`. |
| `M19` | `d58abb6f` | `#1814` | post-xref | `P3` | `S` | `T3` | `Reduce amount of build log spam`. Compiler warning cleanups across compiled MM TUs (`UIWidgets.hpp`, `ActorBehavior.cpp`, `DrawFuncs.cpp`, `Logic.h`, `OnCycleSave.cpp`, `resource/importer/*.h`, `games/mm/CMakeLists.txt`). Take only compiled files; skip `.github/workflows/main.yml` and excluded `BenGui/BenMenu.cpp` / `CosmeticEditor.h`. |

## Needs a ruling (16 commits)

These items cross at least one boundary requiring an explicit ruling (new player-facing option, moving generated rando worlds/goldens, changing save format/JSON, or altering UI/tracker layouts).

### Ship of Harkinian — 6 commits

| Commit | Upstream PR | Xref | Bundle | Ruling needed & technical findings |
|---|---|---|---|---|
| `c724b412` | `#7341` | pre-xref (`#7423`) | `O30` | **Player-facing wincon options + `GID_TRIFORCE` enum collision.** `RSK_WINCON` does not exist in RSBS (replaced by single-exe combo wincon settings). Worse, upstream adds `GID_TRIFORCE` at `0x7D` in `z64item.h`, which collides directly with RSBS's `GID_ROCS_FEATHER` at `games/oot/include/z64item.h:604`. Decide whether any piece (full triforce draw / altar hint) is wanted without `RSK_WINCON`; if yes, remap the `GID_` enum cleanly. |
| `eea5129a` | `#7361` | pre-xref (`#7423`) | `O19` | **Item tracker layout + `#1530` TCG key ruling.** `randomizer_item_tracker.cpp` is retired in RSBS (`SohGui.cpp:219`); unified `ItemAdapterSingleExe.cpp` already tracks `SCENE_THIEVES_HIDEOUT` but omits `SCENE_TREASURE_BOX_SHOP`, and `#1530` intentionally pins TCG out. Needs a ruling to reverse `#1530` and add TCG keys to the unified combo tracker. |
| `8ccdbb98` | `#7385` | pre-xref (`#7423`) | `O13` | **`sohStats` JSON field addition.** RSBS already stops `TIMESTAMP_TRIFORCE_COMPLETED` and sets `gameComplete = 1` at pickup in `OoT_ComboGoal_TriforceHuntEnds` (`games/oot/soh/Enhancements/randomizer/randomizer.cpp:4456-4475`), and `RG_TRIFORCE` does not exist. The only remaining delta is `SaveData`/`LoadData("gameComplete", ...)` in `games/oot/soh/Enhancements/gameplaystats.cpp`, which alters serialized `sohStats` JSON. Rule whether `gameComplete` should serialize. |
| `28ab1206` | `#7396` | pre-xref (`#7423`) | `O28` | **Moves OoT MQ worlds + depends on unported `O28`.** The MQ Light Trial `FAKE_FINAL_ROOM` / empty `FINAL_ROOM` exits are already fixed in `games/oot/soh/Enhancements/randomizer/location_access/dungeons/ganons_castle.cpp:727-743`. The Fire Trial hunk (`RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED`) depends on the unported `O28` region split in `ganons_castle.cpp:494-505` (`2026-10.md` `#1896`) and changes MQ seed generation / goldens. |
| `9ffa4c86` | `#7404` | pre-xref (`#7423`) | `O30` | **Paired wincon timer precedence & Ganon defeat castle warp.** `RSK_WINCON` does not exist in RSBS (`BossDefeatTimestamps.cpp:16-20` gates on `OoT_ComboGoal_OnGanonDefeated`, and `ComboGoalEndingOoT.cpp:85-98` redirects withheld endings to `ENTR_GANONS_TOWER_0`). However, `games/oot/soh/Enhancements/gameplaystats.h:28-30` still checks `TIMESTAMP_DEFEAT_GANON` before `TIMESTAMP_TRIFORCE_COMPLETED`, and `hook_handlers.cpp` lacks the `Entrance_OverrideNextIndex(ENTR_OUTSIDE_GANONS_CASTLE_1_2)` wrap. Rule on timer priority & castle-warp ER interaction under combo goals. |
| `156c2a4a` | `#7417` | pre-xref (`#7423`) | `O28` | **Moves OoT worlds + partial overlap (`#1493`, `#1564`, `#1419`).** `hyrule_field.cpp:17-64` grass `SummonEpona()` is already landed (`d2504828b`, `#1493`). Remaining absent logic hunks — `lake_hylia.cpp` (`SummonEpona()`), `gerudo_valley.cpp:102` (`CanRecoilHover`/`RG_CLIMB`) + `RC_GV_ROCK_ACROSS_BRIDGE_*` (rocks in flight in `#1564`), `forest_temple.cpp`, `jabujabus_belly.cpp`, `spirit_temple.cpp` — move OoT worlds; `gerudo_fortress.cpp` (`RC_GF_GTG_ENTRANCE_RECTANGLE_SIGN`) belongs to parked `#1419` (Shuffle Signs). |

### 2Ship2Harkinian — 10 commits

| Commit | Upstream PR | Xref | Bundle | Ruling needed & technical findings |
|---|---|---|---|---|
| `5fbb86ab` | `#1954` | post-xref (`#2020`) | `M3` | **Moves MM worlds.** Updates Twin Islands scarecrow logic (`North.cpp`) to `CAN_HOOK_SCARECROW`; requires golden re-pin if approved. |
| `58b5dd81` | `#1952` | post-xref (`#2020`) | `M10`/`M12` | **New player-facing option + pause/owl save interaction.** Adds `gEnhancements.Fixes.OwlSaveSceneLayers` + `games/mm/src/code/z_sram_NES.c:1671+` to load scene layers on non-SoT saves; interacts directly with RSBS unified Owl/Pause save handling (`z_sram_NES.c:1718-1744`). |
| `e4061b6f` | `#1984` | post-xref (`#2020`) | `M3` | **Moves MM worlds (`TerminaField.cpp`).** Adjusts Termina Field wall wonder item check time window (`IS_NIGHT()` in `TerminaField.cpp:545` sits on `_09` vs `_12`; verify interaction with in-flight `#1558` Wonder Items). |
| `b43af818` | `#2006` | post-xref (`#2020`) | `M3` | **Partially here (`PiratesFortress.cpp` landed in `#1516`); remaining 7 files move MM worlds.** `Central.cpp`, `East.cpp`, `Moon.cpp`, `StoneTowerTemple.cpp`, `TerminaField.cpp`, `West.cpp`, `WoodfallTemple.cpp` plus `RE_STONE_TOWER_TEMPLE_DEEP_POOL_UNDERWATER_SWITCH` in `Types.h` are absent and move MM worlds. |
| `224ec437` | `#2017` | post-xref (`#2020`) | `M3` | **Moves MM worlds.** Adds `RE_MEET_KOTAKE_WOODS` in `Types.h` and updates `South.cpp` + `StoneTowerTemple.cpp`. |
| `a9a6009a` | `#2018` | post-xref (`#2020`) | `M3` | **Moves MM worlds.** Adjusts `BeneathTheWell.cpp` grass logic; requires golden re-pin if approved. |
| `eb89f12b` | `#1870` | post-xref | `M12` | **New/replaces player-facing option (`ActiveItemOnB`), config updater, & `#1417` seam.** Replaces `GreatFairySwordBButton.cpp` with `gEnhancements.Equipment.ActiveItemOnB`, adds `ConfigVersion2Updater` (`ConfigUpdaters.cpp`), and touches B-button item handling flagged as a seam risk for `#1417` parked `MFE-34` (Song Items) / `MFE-35` (Bomb Arrows). |
| `a0e14e87` | `#1972` | post-xref | `M12` | **New player-facing option.** Adds `gEnhancements.Cycle.MayorArguesUntilMoonfall` + hosted MM row (`SohMenuComboMmEnhancements.cpp` pattern); requires `6080354c` (`#2010`) hook. |
| `31041a7f` | `#1978` | post-xref | `M6` | **New rando option (`RO_ADDITIONAL_ITEMS`) + save layout mutation.** Adds `u16 additionalItemCounts[RI_MAX]` to `RandoSaveInfo` in `z64save.h`, UI, and pool generation; alters combo frozen options and MM save layout. |
| `fd9fb03b` | `#1989` | post-xref | `M12` | **New player-facing option.** Adds `gEnhancements.Items.HotWaterNeverCools` + `VB_COOL_HOT_WATER` in `games/mm/src/code/z_parameter.c:8686` + hosted MM row. |

## Parked (`#1418` / `#1419`) (1 commit)

| Commit | Repo | Upstream PR | Xref | Parked scope | Notes & live RSBS observation |
|---|---|---|---|---|---|
| `9adbdf1f` | SoH | `#7458` | post-xref (`#7479`) | `#1418` (`SFE-26` Ice Trap Names + Teleport Trap) | `Traps.cpp` (`8dd03ef09`) is intentionally skipped/parked in RSBS. Note however that live RSBS still has the `"Power Bracelet"` name collision between trap (`games/oot/soh/Enhancements/randomizer/3drando/shops.cpp:489`) and `GI_STRENGTH_1` (`:1637`). |

## Not applicable to single-exe (20 commits)

### Ship of Harkinian — 15 commits

| Commit | Upstream PR | Xref | Reason N/A to single-exe |
|---|---|---|---|
| `0f1f31a1` | `#7339` | pre-xref (`#7423`) | Standalone `soh/soh/Extractor/Extract.cpp` Linux file-picker dialog; RSBS handles asset extraction / launch outside `Extract.cpp`. |
| `2f9e96c7` | `#7349` | pre-xref (`#7423`) | Modifies `SkipKingZoraMoveOver.cpp` from excluded PR `#7199`; RSBS uses inline `z_en_kz.c:413-466`. |
| `c2325bd7` | `#7370` | pre-xref (`#7423`) | Touches `customequipment.cpp` (excluded standalone enhancement). |
| `88f98c63` | `#7362` | pre-xref (`#7423`) | Anchor multiplayer netplay fixes (unlit per `#1272`). |
| `5c6006ee` | `#7376` | pre-xref (`#7423`) | Standalone `Extractor` + `OTRGlobals::RunExtract` for 64 MB padded MQ ROMs. |
| `e3ff1530` | `#7379` | pre-xref (`#7423`) | Upstream SoH `libultraship` submodule pin bump; RSBS pins `penelopeduncan/libultraship.git` separately. |
| `71461430` | `#7350` | pre-xref (`#7423`) | Touches `BombchuBowling.cpp` from `O36` excluded/standalone minigame module (`#6968`). |
| `7aa53de9` | `#7397` | pre-xref (`#7423`) | Upstream `CMake/automate-vcpkg.cmake` runner script. |
| `2064618b` | `#7409` | pre-xref (`#7423`) | Touches `customequipment.cpp` (excluded standalone enhancement). |
| `62ddf348` | `#7416` | pre-xref (`#7423`) | Upstream SoH `libultraship` submodule pin bump. |
| `b55a687f` | `#7418` | pre-xref (`#7423`) | Upstream SoH `README.md` link updates. |
| `a50c888f` | — (`#7427`) | pre-xref | Upstream SoH `9.3.1` version bump in `soh/CMakeLists.txt`. |
| `d804792b` | `#7426` | pre-xref (`#7431`) | Touches `BombchuBowling.cpp` (uncompiled/excluded standalone minigame enhancement). |
| `f0599f96` | `#7430` | pre-xref (`#7431`) | Upstream SoH `9.3.1` version bump in `soh/CMakeLists.txt`. |
| `a28c6969` | `#7478` | post-xref (`#7479`) | Upstream `.github/workflows/generate-builds.yml` macOS runner bump. |

### 2Ship2Harkinian — 5 commits

| Commit | Upstream PR | Xref | Reason N/A to single-exe |
|---|---|---|---|
| `131cb07a` | `#1996` | pre-xref | Upstream `.github/workflows/clang-format.yml` runner pin. |
| `da2cd965` | `#2002` | pre-xref | Upstream 2Ship `README.md` mod tooling links. |
| `b30a286c` | `#2004` | pre-xref | Upstream 2Ship `CMakeLists.txt` version bump (`Beatrice Alfa`). |
| `2ee1fe12` | `#2016` | post-xref | `mm/src/code/main.c` `SDL_main` return value; replaced by unified `rsbs/src/main.cpp`. |
| `e9a56366` | `#2019` (`#1996`) | post-xref (`#2020`) | Cherry-pick of `#1996` `.github/workflows/clang-format.yml` onto `develop-battler`. |

## Already here (13 commits)

Every commit below was verified present in RSBS `origin/main` (`a28d658a4`) by direct code inspection:

### Ship of Harkinian — 11 commits

| Commit | Upstream PR | Xref | Verified RSBS location & provenance |
|---|---|---|---|
| `430f7cde` | `#7328` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/randomizer/ShuffleSilver.cpp:378-382` (landed in `1c26643db`, PR `#1512`). |
| `4ec74a4d` | `#7338` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/timesaver_hook_handlers.cpp:443` (landed in `b7334e740`, PR `#1490`). |
| `41702557` | `#7340` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/randomizer/ShuffleFairies.cpp:65,73-74,85-87` (present since initial tree `639ea0d8b`). |
| `7e510f69` | `#7363` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/TimeSavers/SkipCutscene/GanonIntros.cpp:37` + `games/oot/src/overlays/actors/ovl_Boss_Ganon/z_boss_ganon.c:908,932,975` (`b7334e740`, PR `#1490`). |
| `0a8b53a7` | `#7368` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/randomizer/location_access/dungeons/jabujabus_belly.cpp:544` (`0de6edfef`, PR `#312`). |
| `e65fa741` | `#7387` | pre-xref (`#7423`) | `games/oot/src/code/audio_seqplayer.c:1190` (`b7334e740`, PR `#1490`). |
| `ce6edf82` | `#7403` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/randomizer/dungeon.cpp:174-177`, `dungeon.h:83`, `randomizer.cpp:4280,4302,4306` (`23e8bbb2b`, PR `#1458`). |
| `d657862a` | `#7413` | pre-xref (`#7423`) | `rsbs/src/main.cpp:601` (`0514e1778`, PR `#1464`). |
| `80f2ad38` | `#7419` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/SpeedModifiers.cpp:73-74` (`b7334e740`, PR `#1490`). |
| `0017fc55` | `#7446` | post-xref (`#7479`) | `rsbs/windows/redship.manifest:10` + `CMakeLists.txt:332` (`0514e1778`, PR `#1464`). |
| `17a38e6b` | `#7456` | post-xref (`#7479`) | `games/oot/soh/Enhancements/randomizer/location_access/overworld/gerudo_valley.cpp:44` (`d2504828b`, PR `#1493`). |

### 2Ship2Harkinian — 2 commits

| Commit | Upstream PR | Xref | Verified RSBS location & provenance |
|---|---|---|---|
| `99bb6318` | `#1962` | pre-xref | `games/mm/2s2h/Rando/ActorBehavior/EnToto.cpp:46-56`, `Logic.h:441,517,622-625`, `Central.cpp:440`, `West.cpp:374` (`ebd5107e2`, PR `#1510`). |
| `6d15e93d` | `#1982` | pre-xref | `games/mm/2s2h/Enhancements/DifficultyOptions/NoDropDespawning.cpp:1-18`, `games/mm/CMakeLists.txt:737` (converged onto `gCheats.DropsDontDie`, `b7334e740`, PR `#1490`). |

## Suggested wave order & proposed `[backport]` packets

Ready without rulings (`T3`, non-world-moving, no new player-facing options):

1. **Wave 1 (`O36` + `O21` + `O35` — zero cross-repo dependencies):**
   - `[backport] SoH vanilla-magic / Barinade hook boundary & RemoveAllQueuedHooks (O36)` — `f93e3cfb` (`#7415`), `41ae6cd3` (`#7422`).
   - `[backport] SoH randomizer typo strings & save editor name encoding (O21/O35)` — `4a1b846c` (`#7357`), `5c59f847` (`#7383`).
2. **Wave 2 (`O13` + `M19` + `M20`):**
   - `[backport] SoH defensive older-save JSON & sohStats reads (O13)` — `4451710b` (`#7434`).
   - `[backport] 2Ship MM actor half-day hook & build warning cleanup (M19)` — `6080354c` (`#2010`), `d58abb6f` (`#1814`).
   - `[backport] 2Ship MM text mod support (M20)` — `faa1e13b` (`#2000`), `222acd18` (`#2013`).
3. **Wave 3 (`L1` fork PR + submodule pin):**
   - `[backport] libultraship tagged ARM64 pointer mask in Fast3D (L1)` — `22aec8f3` (`#1228`), fork PR in `penelopeduncan/libultraship` followed by RSBS submodule pin bump.
4. **Deferred behind rulings (`Needs a ruling` batches):**
   - **Batch A (OoT logic + `O28` prereq):** `28ab1206` (`#7396`, waits on `O28` Fire Trial split), `156c2a4a` (`#7417`, excludes parked `#1419` sign hunk, coordinates `#1564` GV rocks).
   - **Batch B (MM logic `M3` batch):** `5fbb86ab` (`#1954`), `e4061b6f` (`#1984`), `b43af818` (`#2006` remaining 7 files), `224ec437` (`#2017`), `a9a6009a` (`#2018`).
   - **Batch C (OoT wincon / timer / tracker rulings):** `c724b412` (`#7341`), `8ccdbb98` (`#7385`), `9ffa4c86` (`#7404`), `eea5129a` (`#7361`).
   - **Batch D (MM option / save rulings):** `58b5dd81` (`#1952`), `eb89f12b` (`#1870`), `a0e14e87` (`#1972`), `31041a7f` (`#1978`), `fd9fb03b` (`#1989`).

## Appendix A: Complete unpacked commit index (60 non-merge commits across 21 first-parent entries)

### Ship of Harkinian (`dd702b067..d4bf6528a`, 5 first-parent, 38 non-merge)

- `aa89cc57` (2026-10-18, `Merge pull request #7423 from serprex/merge-dewey`, pre-xref) unpacks to **29** commits:
  `430f7cde` (`#7328`, Already here), `4ec74a4d` (`#7338`, Already here), `0f1f31a1` (`#7339`, N/A), `41702557` (`#7340`, Already here), `c724b412` (`#7341`, Needs ruling), `2f9e96c7` (`#7349`, N/A), `4a1b846c` (`#7357`, Bundle `O21`), `eea5129a` (`#7361`, Needs ruling), `7e510f69` (`#7363`, Already here), `0a8b53a7` (`#7368`, Already here), `c2325bd7` (`#7370`, N/A), `88f98c63` (`#7362`, N/A), `5c6006ee` (`#7376`, N/A), `e3ff1530` (`#7379`, N/A), `5c59f847` (`#7383`, Bundle `O35`), `8ccdbb98` (`#7385`, Needs ruling), `e65fa741` (`#7387`, Already here), `71461430` (`#7350`, N/A), `28ab1206` (`#7396`, Needs ruling), `7aa53de9` (`#7397`, N/A), `9ffa4c86` (`#7404`, Needs ruling), `2064618b` (`#7409`, N/A), `ce6edf82` (`#7403`, Already here), `d657862a` (`#7413`, Already here), `62ddf348` (`#7416`, N/A), `156c2a4a` (`#7417`, Needs ruling), `b55a687f` (`#7418`, N/A), `80f2ad38` (`#7419`, Already here), `f93e3cfb` (`#7415`, Bundle `O36`).
- `d458af71` (2026-10-19, `Merge pull request #7427 from f1ana/dewey-bravo`, pre-xref) unpacks to **1** commit: `a50c888f` (N/A).
- `41ae6cd3` (2026-10-19, `#7422`, direct commit, pre-xref): **1** commit: `41ae6cd3` (Bundle `O36`).
- `2827fc2a` (2026-10-20, `Merge pull request #7431 from HarbourMasters/develop-dewey`, pre-xref) unpacks to **2** commits: `d804792b` (`#7426`, N/A), `f0599f96` (`#7430`, N/A).
- `d4bf6528` (2026-11-01, `Merge pull request #7479 from HarbourMasters/develop-dewey`, post-xref) unpacks to **5** commits: `4451710b` (`#7434`, Bundle `O13`), `0017fc55` (`#7446`, Already here), `17a38e6b` (`#7456`, Already here), `9adbdf1f` (`#7458`, Parked `#1418`), `a28c6969` (`#7478`, N/A).

### 2Ship2Harkinian (`f1dda4c28..fd9fb03bd`, 15 first-parent, 21 non-merge)

- Pre-xref (`<= b30a286c5`, 5 direct commits): `131cb07a` (`#1996`, N/A), `da2cd965` (`#2002`, N/A), `99bb6318` (`#1962`, Already here), `6d15e93d` (`#1982`, Already here), `b30a286c` (`#2004`, N/A).
- Post-xref direct commits (9 commits): `faa1e13b` (`#2000`, Bundle `M20`), `6080354c` (`#2010`, Bundle `M19`), `222acd18` (`#2013`, Bundle `M20`), `d58abb6f` (`#1814`, Bundle `M19`), `2ee1fe12` (`#2016`, N/A), `eb89f12b` (`#1870`, Needs ruling), `a0e14e87` (`#1972`, Needs ruling), `31041a7f` (`#1978`, Needs ruling), `fd9fb03b` (`#1989`, Needs ruling).
- `67cc3796` (2026-11-02, `Merge pull request #2020 from 2ship2harkinian/develop-battler`, post-xref) unpacks to **7** commits: `5fbb86ab` (`#1954`, Needs ruling), `58b5dd81` (`#1952`, Needs ruling), `e4061b6f` (`#1984`, Needs ruling), `b43af818` (`#2006`, Needs ruling), `224ec437` (`#2017`, Needs ruling), `e9a56366` (`#2019`/`#1996`, N/A), `a9a6009a` (`#2018`, Needs ruling).

### `libultraship` (`da6daf1a..57e6d409`, 1 first-parent, 1 non-merge)

- `57e6d409` (2026-11-03, `Merge pull request #1228 from pacoa-kdbg/fix/android-arm64-tagged-resource-pointers`, post-xref) unpacks to **1** commit: `22aec8f3` (Bundle `L1`).
