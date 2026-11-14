# Upstream backport delta audit (2026-11)

**Applies to:** `main` at `a28d658a4` (`a28d658a40eb53e0a5e2281a5f080f260319b4a9`). Every presence check and every `path:line` below was made against that sha. This is the follow-up delta audit to [`upstream-backport-audit-2026-10.md`](upstream-backport-audit-2026-10.md); its cutoffs are line 3 of that file, quoted exactly:

> **Applies to:** `main` at `7c1aecf9` (2026-10-26). The finder records were taken against `main` at `96b8ce9a` (2026-10-18); the 80 commits between the two are accounted for in "Changed since the finder snapshot". Import bases: Ship of Harkinian `e149e09e0` (2026-01-11, audited to `dd702b067`), 2Ship2Harkinian `d05565623` (2026-01-10, audited to `f1dda4c28`), libultraship fork merge-base `8c55f607` (2026-01-11, audited to `da6daf1a` on `port-maintenance`).

**Hand-written.** This file was grouped and written by hand: every commit below was read, classified and bundled by hand, and no generator script exists in the tree. It is a planning document for this repository only. Upstream history was read only.

**In-flight backport PRs at `a28d658a4`:** `#1555` and `#1556` have merged; `#1558` (Wonder Items, `#1405`), `#1562` (Icicles / Red Ice, `#1408`), and `#1564` (Rocks / Boulders, `#1409`) remain open in flight and were checked for overlap.

## Scope, cutoffs, and xref boundary

Upstream merge commits were unpacked to individual non-merge commits so fixes folded into batch merges (the merges of SoH `#7423`, `#7427`, `#7431`, `#7479`, 2Ship2Harkinian `#2020`, and libultraship `#1228`) are audited individually with nothing left opaque.

| Repository | Audited range | Tip timestamp | First-parent entries | Unpacked non-merge commits | Notes |
|---|---|---|---|---|---|
| Ship of Harkinian (`develop`) | `dd702b067..d4bf6528a` | `2026-11-01 20:58:28 +0000` | 5 (4 merges and 1 direct commit: `aa89cc57`, `d458af71`, `41ae6cd3` direct, `2827fc2a`, `d4bf6528`) | 38 | `33` `<= 2827fc2a0` (pre-xref); `5` in the merge of `#7479` (post-xref) |
| 2Ship2Harkinian (`develop`) | `f1dda4c28..fd9fb03bd` | `2026-11-02 17:57:36 -0600` | 15 (14 direct commits and 1 merge, `67cc3796`, the merge of `#2020`) | 21 | `5` first-parent `<= b30a286c5` (pre-xref); `16` post-xref |
| libultraship upstream (`port-maintenance`) | `da6daf1a..57e6d409` | `2026-11-03 07:59:06 -0500` | 1 (the merge of `#1228`, `57e6d409`) | 1 | `22aec8f3` merged post-xref; our `libultraship` submodule is this repository's libultraship fork, pinned at `05ae63384` |
| **Total** | | | **21** | **60** | |

Per [`upstream-issue-xref-2026-10-26.md`](upstream-issue-xref-2026-10-26.md#L5-L6), the xref read stopped at SoH `2827fc2a0` (2026-10-20), 2Ship2Harkinian `b30a286c5` (2026-10-24), and libultraship `da6daf1a`. Every commit in this delta is tagged `pre-xref` or `post-xref` relative to those boundaries.

## Classes, calls, and the not-applicable rule

Every unpacked non-merge commit is in exactly one class:

- **Bundle for backport:** ready to port without a ruling, with a priority (P0-P3) and effort (S/M/L/XL) as defined in [`upstream-backport-audit-2026-10.md`](upstream-backport-audit-2026-10.md) line 10, and an existing or new bundle id.
- **Needs a ruling:** moves a world (goldens), changes the save layout or a serialized format, or adds or replaces a player-facing option.
- **Deferred:** the code it changes exists in our build, but the commit waits on an unported prerequisite, or porting it now is not wanted (the reason is given per row).
- **Parked:** inside a parked scope (`#1417`, `#1418`, `#1419`) or a parked item (`#574` and the others listed in `#1566`).
- **Not applicable to single-exe:** only when the file or feature it changes is excluded from the single-executable build (`CMake/SingleExecutable.cmake`, `games/oot/CMakeLists.txt`, `games/mm/CMakeLists.txt`, `rsbs/src/main.cpp`) or the code it changes does not exist here (upstream CI, upstream READMEs, upstream submodule pointers).
- **Already here:** present by symbol, with our `path:line` and the PR that brought it.

The **Call** column uses the 2026-10 vocabulary: `port`, `port-with-adaptation`, `defer` (waits on a prerequisite or a ruling), `skip` (not wanted, reason given).

## Summary counts by class

| Class | Ship of Harkinian | 2Ship2Harkinian | libultraship | Total |
|---|---:|---:|---:|---:|
| Bundle for backport | 8 | 4 | 1 | 13 |
| Needs a ruling | 6 | 10 | 0 | 16 |
| Deferred | 7 | 0 | 0 | 7 |
| Parked (`#1418`, `#574`) | 2 | 0 | 0 | 2 |
| Not applicable to single-exe | 4 | 5 | 0 | 9 |
| Already here | 11 | 2 | 0 | 13 |
| **Total** | **38** | **21** | **1** | **60** |

## Changes to earlier bundles (`2026-10` cross-check)

These commits touch, supersede, or partially land earlier bundles in [`upstream-backport-audit-2026-10.md`](upstream-backport-audit-2026-10.md):

- **`L1` (`67861a90` `#1042` / `3097debf` `#1052` / `bbb565bd` `#1176`):** libultraship `22aec8f3` (merge of `#1228`, `57e6d409`) widens Fast3D 64-bit resource pointer masking in `src/fast/interpreter.cpp:4795-4813` (`(i & 0x00FFFFFFFFFFFFFFull)`) so ARM64 tagged user-space pointers resolve instead of trapping. Lands as a PR against this repository's libultraship fork; add to `L1`.
- **`O13` (`SaveManager` & gameplay stats):** SoH `4451710b` (`#7434`) adds defensive `.value(...)` / `sohStats` JSON checks when loading older saves (`games/oot/soh/SaveManager.cpp:1339,1376,1391`) — bundle-ready; SoH `8ccdbb98` (`#7385`) adds `gameComplete` to `gameplaystats.cpp` JSON save/load (`Needs a ruling`).
- **`O14` (timesaver and cutscene-skip correctness):** SoH `2f9e96c7` (`#7349`) restores the camera and Link's cutscene action when the Mweep cutscene is skipped while in progress. Upstream fixes the hook file from `#7199` (`SkipKingZoraMoveOver.cpp`, which we do not have); the same defect is inline in our `games/oot/src/overlays/actors/ovl_En_Kz/z_en_kz.c:460-467`, so it is bundled as an adapted fix.
- **`O21` (small randomizer cleanups):** SoH `4a1b846c` (`#7357`) fixes remaining `"Southeast Boulder Super"` / `"Notheast Boulder Silver"` typo strings in `ShuffleSilver.cpp:335-336` alongside Fire Trial silver rupee (`430f7cde` `#7328`, already landed in `#1512`), `en_US.json` trick text, and comments — add to `O21`.
- **`O26` (build, extraction and generation):** SoH `0f1f31a1` (`#7339`) shows a message when no Linux file picker is available; SoH `5c6006ee` (`#7376`) adds a failed-extraction popup to `RunExtract` (its Torch hunks do not apply to our ZAPD extractor). Both touch compiled code (`games/oot/CMakeLists.txt:198-214` re-adds `soh/Extractor/*`) and are bundled.
- **`O28` & `#1493` (OoT region/logic fixes):** SoH `28ab1206` (`#7396`) pairs an MQ Light Trial dead-exit cleanup already in RSBS (`games/oot/soh/Enhancements/randomizer/location_access/dungeons/ganons_castle.cpp:727-743`) with an MQ Fire Trial fix (`RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED`) blocked on the unported `O28` `ganons_castle.cpp` region split (`:494-505`). SoH `156c2a4a` (`#7417`) already had its `hyrule_field.cpp:17-64` grass `SummonEpona()` hunk landed by `#1493` (`d2504828b`); its remaining hunks require ruling (and overlap `#1419` Shuffle Signs and `#1564` Rocks).
- **`O30` (win condition / Ganon warp / `gameplaystats.h`):** SoH `c724b412` (`#7341`) and `9ffa4c86` (`#7404`) touch win-condition options, altar hints, `GID_TRIFORCE`, `BossDefeatTimestamps.cpp`, `gameplaystats.h`, and Ganon defeat castle-warp override; both move into `Needs a ruling`.
- **`O32` (custom equipment):** SoH `c2325bd7` (`#7370`) and `2064618b` (`#7409`) add functions to upstream's post-`#6708` `customequipment.cpp`. Our `games/oot/soh/Enhancements/customequipment.cpp` is compiled into the single executable, but it is the pre-rewrite table-based version; the `#6708` rewrite (`0e5083931`) is deferred at [`upstream-backport-audit-2026-10.md:1191`](upstream-backport-audit-2026-10.md#L1191). Both are `Deferred` behind it.
- **`O33` / parked `#574` (Anchor):** SoH `88f98c63` (`#7362`) fixes a use-after-free on Anchor dummy players and syncs silver rupees and auto-planted beans. Anchor is compiled and its page draws, so this is `Parked` under `#574`, not not-applicable.
- **`O35` (QoL / debuggers):** SoH `5c59f847` (`#7383`) fixes `debugSaveEditor.cpp` player-name encoding (`SohGui.cpp:177-179` still exposes the debug save editor in single-exe).
- **`O36` (vanilla/rando boundary + hook hygiene):** SoH `f93e3cfb` (`#7415`) fixes `gRandomizerFlag` magic / Barinade soul hookification; `41ae6cd3` (`#7422`) moves `GameInteractor::RemoveAllQueuedHooks` out of header into `.cpp`. SoH `71461430` (`#7350`) and `d804792b` (`#7426`) change upstream's `BombchuBowling.cpp`, which only exists after the unported `O36` `#6968` (`236c9c81f`, [`upstream-backport-audit-2026-10.md:1277`](upstream-backport-audit-2026-10.md#L1277)); our Bombchu Bowling ammo option is inline (`games/oot/src/overlays/actors/ovl_En_Bom_Bowl_Man/z_en_bom_bowl_man.c:325-327`), so both are `Deferred` extending `O36`.
- **`M3` (MM logic fixes):** 2Ship2Harkinian `b43af818` (`#2006`) already had its `PiratesFortress.cpp` fix landed via PR `#1516` (`a46e8f212`); the other 7 files plus `5fbb86ab` (`#1954`), `e4061b6f` (`#1984`), `224ec437` (`#2017`), and `a9a6009a` (`#2018`) await world-moving rulings.
- **`M12` (MM enhancements & `GreatFairySwordBButton`):** 2Ship2Harkinian `eb89f12b` (`#1870`, `Needs a ruling`) supersedes `GreatFairySwordBButton.cpp` with `gEnhancements.Equipment.ActiveItemOnB` and adds `ConfigVersion2Updater`; `6080354c` (`#2010`, bundleable hook prerequisite) refactors `TingleAlwaysInClockTown` (whose `TingleAlwaysInClockTown.cpp` file is absent/unported from `M12` `f22849e3a` `#1690` per `docs/upstream-backport-audit-2026-10.md:1604`) and adds `OnActorEntryHalfDaysBits` needed by `a0e14e87` (`#1972`).
- **`M19` (build hygiene):** 2Ship2Harkinian `d58abb6f` (`#1814`) strips compiler warnings in compiled MM TUs; `M20` is opened below for MM text mod support (`faa1e13b` `#2000` + follow-up cleanup `222acd18` `#2013`).

## Bundle for backport (13 commits)

None of the items in this class add player-facing options, move generated rando worlds/goldens, or alter serialized save layouts.

### libultraship (`port-maintenance`) — 1 commit

| Bundle | Commit | Upstream PR | Title | Xref | Priority | Effort | Call | Suggested tier | Note & RSBS target |
|---|---|---|---|---|---|---|---|---|---|
| `L1` | `22aec8f3` (merge `57e6d409`) | `#1228` | fix(fast): accept tagged arm64 resource pointers | post-xref | `P2` (`P0` on macOS/Android ARM64 with TBI/MTE) | `S` | `port` | `T3` | Masks `(i & 0x00FFFFFFFFFFFFFFull)` in `src/fast/interpreter.cpp:4795-4813`. Lands as a PR against this repository's libultraship fork (`05ae63384`), then a submodule pin bump here. |

### Ship of Harkinian — 8 commits

| Bundle | Commit | Upstream PR | Title | Xref | Priority | Effort | Call | Suggested tier | Note & RSBS target |
|---|---|---|---|---|---|---|---|---|---|
| `O21` | `4a1b846c` | `#7357` | Fix typos in randomizer | pre-xref (`#7423`) | `P3` | `S` | `port` | `T3` | Fixes `"Southeast Boulder Super"` / `"Notheast Boulder Silver"` in `games/oot/soh/Enhancements/randomizer/ShuffleSilver.cpp:335-336`, `en_US.json:747,975` trick text, and comments in 9 logic/rando files. |
| `O35` | `5c59f847` | `#7383` | [Bugfix] Use game encoding for name in save editor | pre-xref (`#7423`) | `P3` | `S` | `port` | `T3` | Fixes PAL/NTSC character decode/encode in `games/oot/soh/Enhancements/debugger/debugSaveEditor.cpp` (live window registered in `SohGui.cpp:177-179`). |
| `O36` | `f93e3cfb` | `#7415` | [Bugfix] Randomizer flag can set vanilla infinite magic | pre-xref (`#7423`) | `P2` | `S` | `port-with-adaptation` | `T3` | Separates vanilla/rando infinite-magic checks via `GIVanillaBehavior` + `hook_handlers.cpp` and hooks Barinade soul drop cleanly (`z_parameter.c`, `z_boss_va.c`). Keep single-exe `OoT_` prefixes if any touched symbol uses them. |
| `O36` | `41ae6cd3` | `#7422` | move GameInteractor::RemoveAllQueuedHooks body to cpp | pre-xref | `P3` | `S` | `port` | `T3` | Moves template/inline body from `GameInteractor.h` to `GameInteractor.cpp` to stop header bloat / ODR noise. |
| `O13` | `4451710b` | `#7434` | [Bugfix] Older saves cannot be read | post-xref (`#7479`) | `P3` | `S` | `port-with-adaptation` | `T3` | Adds defensive `.value(...)` and `sohStats` guards in `games/oot/soh/SaveManager.cpp` (`:1339,1376,1391`). Preserve single-exe `.redsave` atomic temp-write flow (`:1334-1337`) and `rsbs/` save adapters. |
| `O26` | `0f1f31a1` | `#7339` | Show message when no Linux file picker is found | pre-xref (`#7423`) | `P3` | `S` | `port-with-adaptation` | `T3` | Our compiled `games/oot/soh/Extractor/Extract.cpp:332-341` calls `pfd::open_file` with no `pfd::settings::available()` check (declared at `portable-file-dialogs.h:122`), so on a Linux desktop without zenity or kdialog the ROM picker silently returns nothing. Port the guard and error box, with the message text naming RedShipBlueShip instead of Ship of Harkinian. The MM picker in `games/mm/2s2h/Extractor/Extract.cpp:325-326` (compiled, `games/mm/CMakeLists.txt:169-182`) has the same gap; fix both in one packet. |
| `O26` | `5c6006ee` | `#7376` | Fix extracting from MQ rom padded to 64MB | pre-xref (`#7423`) | `P3` | `S` | `port-with-adaptation` | `T3` | Take only the `RunExtract` failure-popup part; the Torch hunks do not apply (see "Extractor notes" below). Adapt to `games/oot/soh/OTRGlobals.cpp:860-970`: record `!extract.CallZapd(...)` in an atomic, show an "Extraction Failed" popup when the task finishes, keep `generatedIsMQ` unset after a failed first ROM, and offer a retry of either type instead of "ROM Extracted. Extract another?" (`:958`). |
| `O14` | `2f9e96c7` | `#7349` | Allow skipping Mweep cutscene in progress | pre-xref (`#7423`) | `P3` | `S` | `port-with-adaptation` | `T3` | Same defect, inline: `EnKz_SetupMweep` creates the cutscene camera and sets cs action 8 only when `VB_PLAY_MWEEP_CS` answers true (`z_en_kz.c:413-429`), but `EnKz_StopMweep` re-asks the VB before restoring them (`:461-465`). Turning on Skip Story Cutscenes mid-Mweep makes `timesaver_hook_handlers.cpp:562-567` answer false, so the gameplay camera stays `CAM_STAT_WAIT` and Link keeps cs action 8. Adapted fix in `z_en_kz.c` only: restore the camera and set cs action 7 in `EnKz_StopMweep` whenever a cutscene camera was created (`this->cutsceneCamera != CAM_ID_MAIN`), not on the VB answer. |

#### Extractor notes (`O26`, `5c6006ee`)

- **Torch hunks.** Upstream's `ValidateAndFixRom` comment change and the `CallTorch` trim (`MB64` -> `MB54`, region byte restore) target the Torch extractor. Ours is the ZAPD extractor: `Extractor::CallZapd` (`games/oot/soh/Extractor/Extract.cpp:667`) passes the ROM's path, not `mRomData`, to ZAPD (`-b`, `:721`), so neither hunk has anything to change here.
- **64 MB padded MQ debug dumps.** By reading (not run): `ValidateRomSize` accepts `MB64` (`Extract.cpp:390`); `ValidateAndFixRom` restores the region byte in memory (`:356-358`) and accepts the three known 64 MB MQ debug CRCs (`:88-90`); `GetZapdVerStr` picks `GC_MQ_D` from the header CRC (`:617-618`), and ZAPD reads the asset offsets, which all lie inside the first 54 MB, from the file. A 64 MB dump whose padding differs from those three fails the CRC check before ZAPD runs; that is the existing `TODO only check the first 54MB` (`:86`), not something `5c6006ee` changes.
- **Failure popup.** `CallZapd` already shows an error box on its own failure paths (`:689`, `:745`, `:765`, `:773`, `:799`) but not when an exception is caught (`:807-811`), and `RunExtract` ignores its return value, so a failed first extraction is followed by "Extraction Complete" and an offer of the opposite ROM type. That part applies and is bundled.

### 2Ship2Harkinian — 4 commits (all post-xref)

| Bundle | Commit | Upstream PR | Title | Xref | Priority | Effort | Call | Suggested tier | Note & RSBS target |
|---|---|---|---|---|---|---|---|---|---|
| `M20` | `faa1e13b` | `#2000` | Add text mod support | post-xref | `P3` | `M` | `port-with-adaptation` | `T3` | Adds custom OTR/O2R MM text replacement in `games/mm/2s2h/resource/importer/TextMMFactory.{h,cpp}`, `resource/type/TextMM.h`, and `z_message_OTR.cpp` (compiled in `games/mm/CMakeLists.txt:798,831`). Register `S2H::ResourceFactoryXMLTextMMV1` for `RESOURCE_FORMAT_XML` `"TextMM"` version 1 in `games/mm/2s2h/GameExports_SingleExe.cpp` next to `:2908` (`BenPort.cpp` is excluded, `games/mm/CMakeLists.txt:244`). |
| `M20` | `222acd18` | `#2013` | Inline single-use ReadEntry and ReadBox | post-xref | `P3` | `S` | `port` | `T3` | Simplifies `TextMMFactory.cpp` right on top of `faa1e13b`; bundle together with `#2000`. |
| `M19` | `6080354c` | `#2010` | Add OnActorEntryHalfDaysBits hook and refactor TingleAlwaysInClockTown | post-xref | `P3` | `S` | `port-with-adaptation` | `T3` | Adds `OnActorEntryHalfDaysBits` to `GameInteractor{.h,.cpp,_HookTable.h}` and `games/mm/src/code/z_actor.c:3658,4038` (prerequisite if `#1972` `a0e14e87` is approved). Note `games/mm/2s2h/Enhancements/Cutscenes/MiscInteractions/TingleAlwaysInClockTown.cpp` is absent/unported in `origin/main` (`M12`). |
| `M19` | `d58abb6f` | `#1814` | Reduce amount of build log spam | post-xref | `P3` | `S` | `port-with-adaptation` | `T3` | Compiler warning cleanups across compiled MM TUs (`UIWidgets.hpp`, `ActorBehavior.cpp`, `DrawFuncs.cpp`, `Logic.h`, `OnCycleSave.cpp`, `resource/importer/*.h`, `games/mm/CMakeLists.txt`). Take only compiled files; skip `.github/workflows/main.yml` and excluded `BenGui/BenMenu.cpp` / `CosmeticEditor.h`. |

## Needs a ruling (16 commits)

These items cross at least one boundary requiring an explicit ruling (new player-facing option, moving generated rando worlds/goldens, changing save format/JSON, or altering UI/tracker layouts). Every row's call is `defer` until its ruling is written down.

### Ship of Harkinian — 6 commits

| Commit | Upstream PR | Xref | Bundle | Call | Ruling needed & technical findings |
|---|---|---|---|---|---|
| `c724b412` | `#7341` | pre-xref (`#7423`) | `O30` | `defer` | **Player-facing wincon options + `GID_TRIFORCE` enum collision.** `RSK_WINCON` does not exist in RSBS (replaced by single-exe combo wincon settings). Worse, upstream adds `GID_TRIFORCE` at `0x7D` in `z64item.h`, which collides directly with RSBS's `GID_ROCS_FEATHER` at `games/oot/include/z64item.h:604`. Decide whether any piece (full triforce draw / altar hint) is wanted without `RSK_WINCON`; if yes, remap the `GID_` enum cleanly. |
| `eea5129a` | `#7361` | pre-xref (`#7423`) | `O19` | `defer` | **Item tracker layout + `#1364` TCG key ruling.** `randomizer_item_tracker.cpp` is retired in RSBS (`SohGui.cpp:219`); unified `ItemAdapterSingleExe.cpp` already tracks `SCENE_THIEVES_HIDEOUT` but omits `SCENE_TREASURE_BOX_SHOP`, and `RSK_SHUFFLE_CHEST_MINIGAME` is intentionally pinned off in paired worlds under `#1364` (`settings.cpp:152-157`). Needs a ruling to reverse `#1364` and add TCG keys to the unified combo tracker. |
| `8ccdbb98` | `#7385` | pre-xref (`#7423`) | `O13` | `defer` | **`sohStats` JSON field addition.** RSBS already stops `TIMESTAMP_TRIFORCE_COMPLETED` and sets `gameComplete = 1` at pickup in `OoT_ComboGoal_TriforceHuntEnds` (`games/oot/soh/Enhancements/randomizer/randomizer.cpp:4456-4475`), while `RG_TRIFORCE` already exists (`randomizerTypes.h:4866`), with `RSK_WINCON` and an `RG_TRIFORCE` wincon branch in `Item_Give` absent (`randomizer.cpp:4433-4477` only handles `RG_TRIFORCE_PIECE`). The only remaining delta is `SaveData`/`LoadData("gameComplete", ...)` in `games/oot/soh/Enhancements/gameplaystats.cpp`, which alters serialized `sohStats` JSON. Rule whether `gameComplete` should serialize. |
| `28ab1206` | `#7396` | pre-xref (`#7423`) | `O28` | `defer` | **Moves OoT MQ worlds + depends on unported `O28`.** The MQ Light Trial `FAKE_FINAL_ROOM` / empty `FINAL_ROOM` exits are already fixed in `games/oot/soh/Enhancements/randomizer/location_access/dungeons/ganons_castle.cpp:727-743`. The Fire Trial hunk (`RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED`) depends on the unported `O28` region split in `ganons_castle.cpp:494-505` (`docs/upstream-backport-audit-2026-10.md:1896`) and changes MQ seed generation / goldens. |
| `9ffa4c86` | `#7404` | pre-xref (`#7423`) | `O30` | `defer` | **Paired wincon timer precedence & Ganon defeat castle warp.** `RSK_WINCON` does not exist in RSBS (`BossDefeatTimestamps.cpp:16-20` gates on `OoT_ComboGoal_OnGanonDefeated`, and `ComboGoalEndingOoT.cpp:85-98` redirects withheld endings to `ENTR_GANONS_TOWER_0`). However, `games/oot/soh/Enhancements/gameplaystats.h:28-30` still checks `TIMESTAMP_DEFEAT_GANON` before `TIMESTAMP_TRIFORCE_COMPLETED`, and `hook_handlers.cpp` lacks the `Entrance_OverrideNextIndex(ENTR_OUTSIDE_GANONS_CASTLE_1_2)` wrap. Rule on timer priority & castle-warp ER interaction under combo goals. |
| `156c2a4a` | `#7417` | pre-xref (`#7423`) | `O28` | `defer` | **Moves OoT worlds + partial overlap (`#1493`, `#1564`, `#1419`).** `hyrule_field.cpp:17-64` grass `SummonEpona()` is already landed (`d2504828b`, `#1493`). Remaining absent logic hunks — `lake_hylia.cpp` (`SummonEpona()`), `gerudo_valley.cpp:102` (`CanRecoilHover`/`RG_CLIMB`) + `RC_GV_ROCK_ACROSS_BRIDGE_*` (rocks in flight in `#1564`), `forest_temple.cpp`, `jabujabus_belly.cpp`, `spirit_temple.cpp` — move OoT worlds; `gerudo_fortress.cpp` (`RC_GF_GTG_ENTRANCE_RECTANGLE_SIGN`) belongs to parked `#1419` (Shuffle Signs). |

### 2Ship2Harkinian — 10 commits

| Commit | Upstream PR | Xref | Bundle | Call | Ruling needed & technical findings |
|---|---|---|---|---|---|
| `5fbb86ab` | `#1954` | post-xref (`#2020`) | `M3` | `defer` | **Moves MM worlds.** Updates Twin Islands scarecrow logic (`North.cpp`) to `CAN_HOOK_SCARECROW`; requires golden re-pin if approved. |
| `58b5dd81` | `#1952` | post-xref (`#2020`) | `M10`/`M12` | `defer` | **New player-facing option + pause/owl save interaction.** Adds `gEnhancements.Fixes.OwlSaveSceneLayers` + `games/mm/src/code/z_sram_NES.c:1671+` to load scene layers on non-SoT saves; interacts directly with RSBS unified Owl/Pause save handling (`z_sram_NES.c:1718-1744`). |
| `e4061b6f` | `#1984` | post-xref (`#2020`) | `M3` | `defer` | **Moves MM worlds (`TerminaField.cpp`).** Adjusts Termina Field wall wonder item check time window (`IS_NIGHT()` in `TerminaField.cpp:545` sits on `_09` vs `_12`; verify interaction with in-flight `#1558` Wonder Items). |
| `b43af818` | `#2006` | post-xref (`#2020`) | `M3` | `defer` | **Partially here (`PiratesFortress.cpp` landed in `#1516`); remaining 7 files move MM worlds.** `Central.cpp`, `East.cpp`, `Moon.cpp`, `StoneTowerTemple.cpp`, `TerminaField.cpp`, `West.cpp`, `WoodfallTemple.cpp` plus `RE_STONE_TOWER_TEMPLE_DEEP_POOL_UNDERWATER_SWITCH` in `Types.h` are absent and move MM worlds. |
| `224ec437` | `#2017` | post-xref (`#2020`) | `M3` | `defer` | **Moves MM worlds.** Adds `RE_MEET_KOTAKE_WOODS` in `Types.h` and updates `South.cpp` + `StoneTowerTemple.cpp`. |
| `a9a6009a` | `#2018` | post-xref (`#2020`) | `M3` | `defer` | **Moves MM worlds.** Adjusts `BeneathTheWell.cpp` grass logic; requires golden re-pin if approved. |
| `eb89f12b` | `#1870` | post-xref | `M12` | `defer` | **New/replaces player-facing option (`ActiveItemOnB`), config updater, & `#1417` seam.** Replaces `GreatFairySwordBButton.cpp` with `gEnhancements.Equipment.ActiveItemOnB`, adds `ConfigVersion2Updater` (`ConfigUpdaters.cpp`), and touches B-button item handling flagged as a seam risk for `#1417` parked `MFE-34` (Song Items) / `MFE-35` (Bomb Arrows). |
| `a0e14e87` | `#1972` | post-xref | `M12` | `defer` | **New player-facing option.** Adds `gEnhancements.Cycle.MayorArguesUntilMoonfall` + hosted MM row (`SohMenuComboMmEnhancements.cpp` pattern); requires `6080354c` (`#2010`) hook. |
| `31041a7f` | `#1978` | post-xref | `M6` | `defer` | **New rando option (`RO_ADDITIONAL_ITEMS`) + save layout mutation.** Adds `u16 additionalItemCounts[RI_MAX]` to `RandoSaveInfo` in `z64save.h`, UI, and pool generation; alters combo frozen options and MM save layout. |
| `fd9fb03b` | `#1989` | post-xref | `M12` | `defer` | **New player-facing option.** Adds `gEnhancements.Items.HotWaterNeverCools` + `VB_COOL_HOT_WATER` in `games/mm/src/code/z_parameter.c:8686` + hosted MM row. |

## Deferred (7 commits)

The code each of these changes is in our single-executable build, so none is not-applicable; each waits on a prerequisite or is not wanted now.

| Commit | Upstream PR | Title | Xref | Bundle | Call | Reason & RSBS target |
|---|---|---|---|---|---|---|
| `c2325bd7` | `#7370` | Add function to customequipment to fix shooting gallery | pre-xref (`#7423`) | `O32` | `defer` | Depends on the deferred `#6708` CustomEquipment rewrite (`0e5083931`, [`upstream-backport-audit-2026-10.md:1191`](upstream-backport-audit-2026-10.md#L1191)). Our compiled `games/oot/soh/Enhancements/customequipment.cpp` is the earlier table-based version, so the bow/slingshot fix would need re-deriving against it; take it with `#6708`, or as an adapted fix if a shooting-gallery bug is seen with a custom-equipment pack. |
| `2064618b` | `#7409` | Add Function to custom equipment to guard against cutscene shenanigans | pre-xref (`#7423`) | `O32` | `defer` | Same dependency as `c2325bd7`: the cutscene left-hand guard is written against the post-`#6708` file and would need re-deriving against our table-based `customequipment.cpp`. |
| `71461430` | `#7350` | Bombchu Bowling ammo changeable during minigame | pre-xref (`#7423`) | `O36` | `defer` | Extends `O36`: upstream changes `BombchuBowling.cpp`, created by the unported `#6968` (`236c9c81f`, [`upstream-backport-audit-2026-10.md:1277`](upstream-backport-audit-2026-10.md#L1277)). Our option is inline: `z_en_bom_bowl_man.c:325-327` reads `BombchuBowlingAmmo` (row at `SohMenuEnhancements.cpp:1566`) only when a game starts. Follow `#6968` when it is ported, or adapt the in-minigame ammo change (apply the CVar delta to `play->bombchuBowlingStatus` while it is positive) to the inline code. |
| `d804792b` | `#7426` | [Bugfix] Bombchu count slider crashes with no game loaded | pre-xref (`#7431`) | `O36` | `defer` | Null-`gPlayState` guard for `71461430`'s new code; our inline path has no such hook today. Take it with `71461430`, whichever way that is ported. |
| `7aa53de9` | `#7397` | Try to fix windows builds | pre-xref (`#7423`) | `O26` | `skip` | Our root `CMakeLists.txt:52-53` includes `CMake/automate-vcpkg.cmake` on Windows, and it still honours `ENV{VCPKG_ROOT}` (`automate-vcpkg.cmake:84-90`), so the code exists. Upstream drops that branch to stop CI runners' preinstalled vcpkg being used; our Windows CI builds pass with it. Take it only if a Windows runner image breaks the same way. |
| `a50c888f` | — (merge of `#7427`) | 9.3.1 Dewey Bravo | pre-xref | — | `skip` | Bumps `project(Ship VERSION)`. Ours is pinned at `9.1.1` by the save-format tripwire at `CMakeLists.txt:9-21`: a Ship version change renames aside every randomizer `.sav` at first launch, so it is a save break decided on its own, never by a backport. |
| `f0599f96` | `#7430` | 9.3.1 | pre-xref (`#7431`) | — | `skip` | Same version bump as `a50c888f`; same tripwire. |

## Parked (2 commits)

| Commit | Repo | Upstream PR | Title | Xref | Parked scope | Notes & live RSBS observation |
|---|---|---|---|---|---|---|
| `9adbdf1f` | SoH | `#7458` | Rename Power Bracelet Trap | post-xref (`#7479`) | `#1418` (`SFE-26` Ice Trap Names + Teleport Trap) | Upstream SoH's `Traps.cpp` (`8dd03ef09`, absent in `origin/main`) is intentionally skipped/parked in RSBS (`games/mm/2s2h/Rando/MiscBehavior/Traps.cpp` is unrelated MM code). Note however that live RSBS OoT still has the `"Power Bracelet"` name collision between trap (`games/oot/soh/Enhancements/randomizer/3drando/shops.cpp:489`) and `GI_STRENGTH_1` (`:1637`). |
| `88f98c63` | SoH | `#7362` | Fix anchor issues | pre-xref (`#7423`) | `#574` (Anchor) | Anchor is compiled (`games/oot/soh/Network/Anchor/`, `soh_port` whole-archived at `games/oot/CMakeLists.txt:305-330`; instance created at `OTRGlobals.cpp:2125`) and its page draws. The commit's use-after-free fix applies to our compiled `DummyPlayer_Destroy` (`games/oot/soh/Network/Anchor/DummyPlayer.cpp:244`): it clears the client's `player` pointer so packet handlers stop using a destroyed dummy. It also syncs silver rupees (`JsonConversions.hpp`) and auto-planted beans (`hook_handlers.cpp`). When `#574` is lifted it joins `O33`, use-after-free fix first. |

## Not applicable to single-exe (9 commits)

Each entry was re-read against the not-applicable rule above.

### Ship of Harkinian — 4 commits

| Commit | Upstream PR | Title | Xref | Call | Reason not applicable |
|---|---|---|---|---|---|
| `e3ff1530` | `#7379` | Pin latest libultraship's port-maintenance branch | pre-xref (`#7423`) | `skip` | Changes only upstream SoH's `libultraship` gitlink (to `b374560b`, before our libultraship cutoff `da6daf1a`). Our gitlink points at this repository's libultraship fork and is bumped on its own; the libultraship commits themselves are audited in the libultraship section. |
| `62ddf348` | `#7416` | push latest LUS to dewey | pre-xref (`#7423`) | `skip` | Same as `e3ff1530` (gitlink to `da6daf1a`, our libultraship cutoff). |
| `b55a687f` | `#7418` | Fix readme links | pre-xref (`#7423`) | `skip` | Changes upstream's README nightly-build links; our `README.md` has no such links (`git grep nightly.link -- README.md`: no hits). |
| `a28c6969` | `#7478` | Update Mac CI Runner | post-xref (`#7479`) | `skip` | Upstream CI (`.github/workflows/generate-builds.yml`). Our workflow is our own and already runs `macos-15` (`.github/workflows/generate-builds.yml:294`). |

### 2Ship2Harkinian — 5 commits

| Commit | Upstream PR | Title | Xref | Call | Reason not applicable |
|---|---|---|---|---|---|
| `131cb07a` | `#1996` | Pin formatting workflow to Ubuntu 24.04 | pre-xref | `skip` | Upstream CI. Our `clang-format` job is our own and pinned to `ubuntu-22.04` for clang-format 14 (`.github/workflows/clang-format.yml:25`). |
| `da2cd965` | `#2002` | update readme mod tooling links | pre-xref | `skip` | Changes upstream's README tooling links; our `README.md` has no such links. |
| `b30a286c` | `#2004` | Bump for Beatrice Alfa | pre-xref | `skip` | Changes upstream's root `project(2s2h VERSION)` and build name. Our MM half builds under the root `project(Ship VERSION 9.1.1)` (`CMakeLists.txt:9`); `games/mm/CMakeLists.txt:5` declares no version, so the code is absent. |
| `2ee1fe12` | `#2016` | Return success from SDL_main after normal shutdown | post-xref | `skip` | MM's `SDL_main` is compiled out of the single executable (`#ifndef RSBS_SINGLE_EXECUTABLE`, `games/mm/src/code/main.c:52-156`); the entry point is `rsbs/src/main.cpp`. |
| `e9a56366` | `#2019` (`#1996`) | Pin formatting workflow to Ubuntu 24.04 (#1996) | post-xref (`#2020`) | `skip` | Cherry-pick of `#1996` onto upstream's `develop-battler` branch; upstream CI, as `131cb07a`. |

## Already here (13 commits)

Every commit below was verified present in RSBS `origin/main` (`a28d658a4`) by direct code inspection:

### Ship of Harkinian — 11 commits

| Commit | Upstream PR | Xref | Verified RSBS location & provenance |
|---|---|---|---|
| `430f7cde` | `#7328` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/randomizer/ShuffleSilver.cpp:378-382` (landed in `1c26643db`, PR `#1512`). |
| `4ec74a4d` | `#7338` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/timesaver_hook_handlers.cpp:443` (landed in `b7334e740`, PR `#1490`). |
| `41702557` | `#7340` | pre-xref (`#7423`) | `games/oot/soh/Enhancements/randomizer/ShuffleFairies.cpp:65,73-74,85-87` (landed in `dd419c37b`, PR `#1292`). |
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

1. **Wave 1 (`O36` + `O21` + `O35` + `O14` + `O26` — zero cross-repo dependencies):**
   - `[backport] SoH vanilla-magic / Barinade hook boundary & RemoveAllQueuedHooks (O36)` — `f93e3cfb` (`#7415`), `41ae6cd3` (`#7422`).
   - `[backport] SoH randomizer typo strings & save editor name encoding (O21/O35)` — `4a1b846c` (`#7357`), `5c59f847` (`#7383`).
   - `[backport] Extractor: Linux file-picker message and failed-extraction popup (O26)` — `0f1f31a1` (`#7339`), `5c6006ee` (`#7376`, `RunExtract` part only).
   - `[backport] Mweep skipped mid-cutscene restores camera and Link (O14)` — `2f9e96c7` (`#7349`, adapted to inline `z_en_kz.c`).
2. **Wave 2 (`O13` + `M19` + `M20`):**
   - `[backport] SoH defensive older-save JSON & sohStats reads (O13)` — `4451710b` (`#7434`).
   - `[backport] 2Ship MM actor half-day hook & build warning cleanup (M19)` — `6080354c` (`#2010`), `d58abb6f` (`#1814`).
   - `[backport] 2Ship MM text mod support (M20)` — `faa1e13b` (`#2000`), `222acd18` (`#2013`).
3. **Wave 3 (`L1` fork PR + submodule pin):**
   - `[backport] libultraship tagged ARM64 pointer mask in Fast3D (L1)` — `22aec8f3` (`#1228`), a PR against this repository's libultraship fork followed by the submodule pin bump here.
4. **Deferred behind rulings (`Needs a ruling` batches):**
   - **Batch A (OoT logic + `O28` prereq):** `28ab1206` (`#7396`, waits on `O28` Fire Trial split), `156c2a4a` (`#7417`, excludes parked `#1419` sign hunk, coordinates `#1564` GV rocks).
   - **Batch B (MM logic `M3` batch):** `5fbb86ab` (`#1954`), `e4061b6f` (`#1984`), `b43af818` (`#2006` remaining 7 files), `224ec437` (`#2017`), `a9a6009a` (`#2018`).
   - **Batch C (OoT wincon / timer / tracker rulings):** `c724b412` (`#7341`), `8ccdbb98` (`#7385`), `9ffa4c86` (`#7404`), `eea5129a` (`#7361`).
   - **Batch D (MM option / save rulings):** `58b5dd81` (`#1952`), `eb89f12b` (`#1870`), `a0e14e87` (`#1972`), `31041a7f` (`#1978`), `fd9fb03b` (`#1989`).
5. **Deferred behind prerequisites:** `c2325bd7` and `2064618b` with `O32` `#6708`; `71461430` and `d804792b` with `O36` `#6968` (or an adapted inline change). `7aa53de9`, `a50c888f` and `f0599f96` are `skip`.

## Appendix A: Complete unpacked commit index (60 non-merge commits across 21 first-parent entries)

Dates are upstream author dates (UTC). Titles are the commit subjects; merge titles are written as "merge of #N".

### Ship of Harkinian (`dd702b067..d4bf6528a`, 5 first-parent entries, 38 non-merge)

**`aa89cc57`** (2026-10-18, merge of `#7423`, pre-xref) unpacks to **29** commits:

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `430f7cde` | 2026-10-07 | `#7328` | Fix name of Fire Trial silver rupee | Already here |
| `4ec74a4d` | 2026-10-08 | `#7338` | Unfreeze Link on forced text skip | Already here |
| `0f1f31a1` | 2026-10-08 | `#7339` | Show message when no Linux file picker is found | Bundle `O26` |
| `41702557` | 2026-10-08 | `#7340` | Prevent crash when fairy spawn is unknown | Already here |
| `c724b412` | 2026-10-08 | `#7341` | Win condition tooltip + altar hint + draw full triforce for anywhere wincon | Needs a ruling |
| `2f9e96c7` | 2026-10-09 | `#7349` | Allow skipping Mweep cutscene in progress | Bundle `O14` |
| `4a1b846c` | 2026-10-10 | `#7357` | Fix typos in randomizer | Bundle `O21` |
| `eea5129a` | 2026-10-10 | `#7361` | Add TCG keys to item tracker | Needs a ruling |
| `7e510f69` | 2026-10-11 | `#7363` | [Bugfix] Restore Ganondorf Lights Hint | Already here |
| `0a8b53a7` | 2026-10-11 | `#7368` | Fix missing open chest logic in Jabu MQ | Already here |
| `c2325bd7` | 2026-10-11 | `#7370` | Add function to customequipment to fix shooting gallery | Deferred (`O32`) |
| `88f98c63` | 2026-10-11 | `#7362` | Fix anchor issues | Parked `#574` |
| `5c6006ee` | 2026-10-12 | `#7376` | Fix extracting from MQ rom padded to 64MB | Bundle `O26` |
| `e3ff1530` | 2026-10-12 | `#7379` | Pin latest libultraship's port-maintenance branch | Not applicable |
| `5c59f847` | 2026-10-13 | `#7383` | [Bugfix] Use game encoding for name in save editor | Bundle `O35` |
| `8ccdbb98` | 2026-10-13 | `#7385` | stop timer upon lifting triforce, not upon warping to credits | Needs a ruling |
| `e65fa741` | 2026-10-13 | `#7387` | Fix OOB check in audio_seqplayer.c | Already here |
| `71461430` | 2026-10-13 | `#7350` | Bombchu Bowling ammo changeable during minigame | Deferred (`O36`) |
| `28ab1206` | 2026-10-14 | `#7396` | Fix logic errors due to unused regions | Needs a ruling |
| `7aa53de9` | 2026-10-14 | `#7397` | Try to fix windows builds | Deferred (skip) |
| `9ffa4c86` | 2026-10-15 | `#7404` | Fix defeating Ganon when wincon is not Defeat Ganon warping outside Hyrule Castle | Needs a ruling |
| `2064618b` | 2026-10-17 | `#7409` | Add Function to custom equipment to guard against cutscene shenanigans | Deferred (`O32`) |
| `ce6edf82` | 2026-10-17 | `#7403` | Fix tracker showing too any keys with skeleton key after using key | Already here |
| `d657862a` | 2026-10-18 | `#7413` | LC_NUMERIC C | Already here |
| `62ddf348` | 2026-10-18 | `#7416` | push latest LUS to dewey | Not applicable |
| `156c2a4a` | 2026-10-18 | `#7417` | Some logic fixes for dewey | Needs a ruling |
| `b55a687f` | 2026-10-18 | `#7418` | Fix readme links | Not applicable |
| `80f2ad38` | 2026-10-18 | `#7419` | fix the jump speed limiter while wearing bunny hood | Already here |
| `f93e3cfb` | 2026-10-18 | `#7415` | [Bugfix] Randomizer flag can set vanilla infinite magic | Bundle `O36` |

**`d458af71`** (2026-10-19, merge of `#7427`, pre-xref) unpacks to **1** commit:

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `a50c888f` | 2026-10-19 | — | 9.3.1 Dewey Bravo | Deferred (skip) |

**`41ae6cd3`** (2026-10-19, direct commit, pre-xref): **1** commit:

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `41ae6cd3` | 2026-10-19 | `#7422` | move GameInteractor::RemoveAllQueuedHooks body to cpp | Bundle `O36` |

**`2827fc2a`** (2026-10-20, merge of `#7431`, pre-xref) unpacks to **2** commits:

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `d804792b` | 2026-10-19 | `#7426` | [Bugfix] Bombchu count slider crashes with no game loaded | Deferred (`O36`) |
| `f0599f96` | 2026-10-19 | `#7430` | 9.3.1 | Deferred (skip) |

**`d4bf6528`** (2026-11-01, merge of `#7479`, post-xref) unpacks to **5** commits:

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `4451710b` | 2026-10-21 | `#7434` | [Bugfix] Older saves cannot be read | Bundle `O13` |
| `0017fc55` | 2026-10-22 | `#7446` | fix UTF-8 paths on Windows | Already here |
| `17a38e6b` | 2026-10-23 | `#7456` | fix GV bean to alcove logic | Already here |
| `9adbdf1f` | 2026-10-24 | `#7458` | Rename Power Bracelet Trap | Parked `#1418` |
| `a28c6969` | 2026-11-01 | `#7478` | Update Mac CI Runner | Not applicable |

### 2Ship2Harkinian (`f1dda4c28..fd9fb03bd`, 15 first-parent entries, 21 non-merge)

**Pre-xref** (`<= b30a286c5`, 5 direct commits):

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `131cb07a` | 2026-10-20 | `#1996` | Pin formatting workflow to Ubuntu 24.04 | Not applicable |
| `da2cd965` | 2026-10-22 | `#2002` | update readme mod tooling links | Not applicable |
| `99bb6318` | 2026-10-24 | `#1962` | [Rando] Fix termina wall ocarina logic | Already here |
| `6d15e93d` | 2026-10-24 | `#1982` | [Enhancement] Drops Don't Despawn | Already here |
| `b30a286c` | 2026-10-24 | `#2004` | Bump for Beatrice Alfa | Not applicable |

**Post-xref direct commits** (9 commits):

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `faa1e13b` | 2026-10-27 | `#2000` | Add text mod support | Bundle `M20` |
| `6080354c` | 2026-10-27 | `#2010` | Add OnActorEntryHalfDaysBits hook and refactor TingleAlwaysInClockTown | Bundle `M19` |
| `222acd18` | 2026-10-29 | `#2013` | Inline single-use ReadEntry and ReadBox | Bundle `M20` |
| `d58abb6f` | 2026-11-02 | `#1814` | Reduce amount of build log spam | Bundle `M19` |
| `2ee1fe12` | 2026-11-02 | `#2016` | Return success from SDL_main after normal shutdown | Not applicable |
| `eb89f12b` | 2026-11-02 | `#1870` | [Enhancement] Trigger active item on B | Needs a ruling |
| `a0e14e87` | 2026-11-02 | `#1972` | Mayor Argues Until Moonfall Enhancement | Needs a ruling |
| `31041a7f` | 2026-11-02 | `#1978` | [Rando] Add Additional Items to the Item Pool | Needs a ruling |
| `fd9fb03b` | 2026-11-02 | `#1989` | [Enhancement] Prevent Hot Spring Water from cooling | Needs a ruling |

**`67cc3796`** (2026-11-02, merge of `#2020`, post-xref) unpacks to **7** commits:

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `5fbb86ab` | 2026-09-16 | `#1954` | Use CAN_HOOK_SCARECROW for Twin Islands scarecrow | Needs a ruling |
| `58b5dd81` | 2026-10-14 | `#1952` | Load appropriate scene layers from non-SoT saves | Needs a ruling |
| `e4061b6f` | 2026-10-24 | `#1984` | Fix logic of Termina Field wall wonder items | Needs a ruling |
| `b43af818` | 2026-11-02 | `#2006` | [Rando] Swim and other Minor Logic Fixes | Needs a ruling |
| `224ec437` | 2026-11-02 | `#2017` | [Rando] Fixed Kotake and Armos Logic | Needs a ruling |
| `e9a56366` | 2026-11-02 | `#2019` (`#1996`) | Pin formatting workflow to Ubuntu 24.04 (#1996) | Not applicable |
| `a9a6009a` | 2026-11-02 | `#2018` | [Rando] Beneath The Well Grass Logic Fix | Needs a ruling |

### libultraship (`da6daf1a..57e6d409`, 1 first-parent entry, 1 non-merge)

**`57e6d409`** (2026-11-03, merge of `#1228`, post-xref) unpacks to **1** commit:

| Commit | Date | Upstream PR | Title | Class |
|---|---|---|---|---|
| `22aec8f3` | 2026-08-04 | `#1228` | fix(fast): accept tagged arm64 resource pointers | Bundle `L1` |
