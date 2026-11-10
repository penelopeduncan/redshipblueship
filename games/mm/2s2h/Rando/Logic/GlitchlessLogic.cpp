#include "Logic.h"
#include "2s2h/Rando/Types.h"

#include <numeric>
#include <iterator>
#include <spdlog/spdlog.h>

extern "C" {
#include "variables.h"
#include "ShipUtils.h"
uint64_t GetUnixTimestamp();
}

#ifdef RSBS_SINGLE_EXECUTABLE
// The creation-progress overlay (#582). This loop is the only place in the paired
// creation that needs a HEARTBEAT — a repaint from inside an open-ended grind —
// because one pass of this fill can occupy the whole per-attempt budget while
// nothing else in it would report.
//
// NOT because everything else finishes in milliseconds; the first cut of #582 said
// that without measuring, and the review was right to disbelieve it. Two other
// stretches are long enough for a player to notice: MM's rando-core init before
// the ladder, and the spoiler join after it. Both are bounded, single calls rather
// than loops, so each got a phase REPORT (which paints once, with a caption naming
// what is happening) instead of a heartbeat, and both are timed on every creation
// — the "[MM] creation: the pre-fill stretch ..." and "... post-fill stretch ..."
// lines in GameExports_SingleExe.cpp — so the claim is a number in the log.
#include "gen_progress_overlay.h"
#endif

namespace Rando {

namespace Logic {

#ifdef RSBS_SINGLE_EXECUTABLE
// Declared in Logic.h. Zero in every shipping path — see the declaration for
// why the lock needs it and why nothing but the lock may write it.
unsigned int gRsbsGlitchlessTimeoutMsOverride = 0;
#endif

void ApplyGlitchlessLogicToSaveContext(std::vector<RandoCheckId>& checkPool, std::vector<RandoItemId>& itemPool) {
    uint64_t tick = GetUnixTimestamp();

    // The fill's wall-clock budget. 10s is upstream's number and the shipping
    // one; the override exists only so the ADR 0010 lock can observe a
    // wall-clock abort without stalling CI for ten real seconds per attempt.
    uint64_t timeoutBudgetMs = 10000;
#ifdef RSBS_SINGLE_EXECUTABLE
    if (gRsbsGlitchlessTimeoutMsOverride != 0) {
        timeoutBudgetMs = (uint64_t)gRsbsGlitchlessTimeoutMsOverride;
    }
#endif

    SaveContext copiedSaveContext;
    memcpy(&copiedSaveContext, &gSaveContext, sizeof(SaveContext));

    std::set<RandoRegionId> regionsInLogic = { RR_MAX };
    std::set<RandoCheckId> checksInLogic;
    std::set<std::pair<RandoEvent, std::function<bool()>>*> eventsInLogic;

    // Initialize time states using shared function
    std::unordered_map<RandoRegionId, RegionTimeState> regionTimeStates = InitializeRegionTimeStates(RR_MAX);

    RandoCheckId checkWithJunk = RC_UNKNOWN;
    std::set<RandoItemId> nonJunkItemsThatWeHaveTried;
    std::vector<RandoCheckId> checksWithJunk;
    std::vector<int> checksWithJunkWeights;
    int weight = 1;

    // Inital shuffle of the item pool (Following shuffles done at the end of the loop)
    if (itemPool.size() > 1) {
        for (size_t i = 0; i < itemPool.size(); i++) {
            size_t j = Ship_Random(0, itemPool.size());
            std::swap(itemPool[i], itemPool[j]);
        }
    }

    // `wallClock` marks the ONE failure below that is not a function of the
    // seeded RNG stream (the timeout at the top of the loop). It is thrown as
    // Rando::Logic::GenerationTimeout so a caller cannot mistake a slow
    // machine for a dead-end seed — see the type's comment in Logic.h and ADR
    // 0010 increment 1.2's attempt ladder, which must not re-roll on it.
    auto handleError = [&](std::string message, bool wallClock = false) {
        SPDLOG_ERROR("Items/Checks: {}/{}", itemPool.size(), checkPool.size());

        // Log out the checks that are still in the pool
        for (auto& randoCheckId : checkPool) {
            SPDLOG_ERROR("Check still in pool: {}", Rando::StaticData::Checks[randoCheckId].name);
        }
        // Log out the items that are still in the pool
        for (RandoItemId randoItemId : itemPool) {
            SPDLOG_ERROR("Item still in pool: {}", Rando::StaticData::Items[randoItemId].spoilerName);
        }

        memcpy(&gSaveContext, &copiedSaveContext, sizeof(SaveContext));
        if (wallClock) {
            throw GenerationTimeout(message);
        }
        throw std::runtime_error(message);
    };

    while (true) {
#ifdef RSBS_SINGLE_EXECUTABLE
        // ------------------------------------------------------------------
        // THE ON-SCREEN HEARTBEAT (#582).
        //
        // The paired creation runs on the thread that renders, so without a
        // pump from in here the window produces no frame and answers no OS
        // message for the whole of this fill. The overlay itself decides how
        // rarely to paint (RSBS_GENOVERLAY_REPAINT_INTERVAL_MS); this loop only
        // offers it the elapsed time it had already computed for the timeout
        // check below, so no new clock and no new decision enter the fill.
        //
        // THE CREDIT IS THE LOAD-BEARING PART. A painted frame waits for
        // vblank, and charging that wait to the fill's wall-clock budget would
        // give a host with a window measurably less generation headroom than
        // the same host running headless — two different abort probabilities
        // for one seed on one machine, decided by whether anything was on
        // screen. So presentation time is added back to `tick`: the budget
        // keeps measuring GENERATION, exactly as it does with no overlay
        // installed. The guard means a headless run does not even read the
        // clock twice.
        //
        // THIS CREDIT IS ONE OF TWO, and it covers only the PER-ATTEMPT stop
        // below. The creation's other wall-clock stop — the ladder's TOTAL
        // budget in OnFileCreate.cpp — is credited by the
        // Combo_GenProgress_PresentationBegin/End bracket inside the overlay's
        // own Paint(), in gen_budget's clock, because `tick` here is a
        // GetUnixTimestamp() value and only a delta in that same clock may be
        // added to it. Crediting one and not the other (the first cut of #582)
        // just moved the windowed-vs-headless asymmetry to the stop nobody was
        // looking at.
        if (ComboGenOverlay_WantsHeartbeat()) {
            const uint64_t beforePaint = GetUnixTimestamp();
            ComboGenOverlay_Heartbeat((uint32_t)(beforePaint >= tick ? beforePaint - tick : 0));
            const uint64_t afterPaint = GetUnixTimestamp();
            // GUARDED, because GetUnixTimestamp() is system_clock and can step
            // BACKWARDS (an NTP correction mid-generation). An unguarded
            // subtraction would underflow to an enormous credit, push `tick`
            // far into the future, and make the check below underflow in turn
            // and abort the fill instantly. A backwards step simply credits
            // nothing, which leaves this loop exactly as exposed to a clock
            // step as it already was and no more.
            if (afterPaint >= beforePaint) {
                tick += afterPaint - beforePaint;
            }
        }
#endif
        // Break if we've been running for too long
        if (GetUnixTimestamp() - tick > timeoutBudgetMs) {
            handleError("Logic Generation Timeout", /*wallClock=*/true);
        }

        bool regionsInLogicChanged = false;
        bool eventsInLogicChanged = false;
        bool checksInLogicChanged = false;
        // Set when this pass places a time item; acted on after the region loop
        // below, which iterates regionsInLogic and reads regionTimeStates (#1509).
        bool timeGained = false;

        // Crawl through all reachable regions and add any new reachable regions
        auto prevRegionsInLogicSize = regionsInLogic.size();
        for (RandoRegionId regionId : regionsInLogic) {
            FindReachableRegions(regionId, regionsInLogic, regionTimeStates);
        }
        if (regionsInLogic.size() != prevRegionsInLogicSize) {
            regionsInLogicChanged = true;
        }

        for (RandoRegionId regionId : regionsInLogic) {
            auto& randoRegion = Regions[regionId];

            // Set current region time for check evaluation
            SetCurrentRegionTime(regionTimeStates, regionId);

            // Apply any new events
            for (auto& randoEvent : randoRegion.events) {
                if (!eventsInLogic.contains(&randoEvent) && randoEvent.second()) {
                    RANDO_EVENTS[randoEvent.first]++;
                    eventsInLogic.insert(&randoEvent);
                    eventsInLogicChanged = true;
                }
            }

            // Apply any new checks
            for (auto& [randoCheckId, checkLogic] : randoRegion.checks) {
                if (!checksInLogic.contains(randoCheckId) && checkLogic.first()) {
                    // VALIDATION: Verify check is reachable with owned time
                    TimeLogic::ValidateRegionTimeOwnership(regionId, randoCheckId,
                                                           regionTimeStates[regionId].timeSlices, "Glitchless");

                    auto it = std::find(checkPool.begin(), checkPool.end(), randoCheckId);
                    bool inPool = it != checkPool.end();
                    checksInLogic.insert(randoCheckId);
                    if (inPool) {
                        checkPool.erase(it);
                    }

                    RandoItemId randoItemId = RANDO_SAVE_CHECKS[randoCheckId].randoItemId;

                    if (inPool) {
                        size_t pickIndex = SelectItemForCheck(itemPool, checkPool, randoCheckId);
                        if (pickIndex == itemPool.size()) {
                            handleError("No allowed item remains for reachable check: " +
                                        std::string(Rando::StaticData::Checks[randoCheckId].name));
                        }

                        randoItemId = itemPool[pickIndex];
                        itemPool.erase(itemPool.begin() + pickIndex);

                        if (Rando::StaticData::Items[randoItemId].randoItemType == RITYPE_JUNK ||
                            Rando::StaticData::Items[randoItemId].randoItemType == RITYPE_HEALTH) {
                            checksWithJunk.push_back(randoCheckId);
                            checksWithJunkWeights.push_back(weight);
                            SPDLOG_TRACE("Item Placed: {}:{}", Rando::StaticData::Checks[randoCheckId].name,
                                         Rando::StaticData::Items[randoItemId].spoilerName);
                        } else {
                            SPDLOG_DEBUG("Item Placed: {}:{}", Rando::StaticData::Checks[randoCheckId].name,
                                         Rando::StaticData::Items[randoItemId].spoilerName);
                        }
                        RANDO_SAVE_CHECKS[randoCheckId].randoItemId = randoItemId;
                        RANDO_SAVE_CHECKS[randoCheckId].shuffled = true;
                    }

                    GiveItem(ConvertItem(randoItemId));

                    // A time item restarts the traversal from the root at the new owned
                    // time (#1509, ResetRegionTimeStatesForTimeGain), after this pass's
                    // region loop. Widening every region's mask in place here undid the
                    // per-edge narrowing (#1453) for every region already discovered.
                    // Also re-evaluate events, since time-gated events may now pass.
                    if (randoItemId >= RI_TIME_DAY_1 && randoItemId <= RI_TIME_PROGRESSIVE) {
                        timeGained = true;
                        regionsInLogicChanged = true;
                        eventsInLogicChanged = true;
                    }

                    checksInLogicChanged = true;
                }
            }
        }

        if (timeGained) {
            ResetRegionTimeStatesForTimeGain(regionsInLogic, regionTimeStates);
        }

        if (itemPool.empty()) {
            // Done!
            break;
        }

        // Choose a random check with junk, and attempt to place progressive items until we unlock something
        if (!regionsInLogicChanged && !checksInLogicChanged && !eventsInLogicChanged) {
            if (checkWithJunk == RC_UNKNOWN) {
                if (checksWithJunk.empty()) {
                    handleError("No checks with junk, not sure what to do");
                }

                if (checksWithJunk.size() == 1) {
                    checkWithJunk = checksWithJunk[0];
                    checksWithJunk.pop_back();
                    checksWithJunkWeights.pop_back();
                } else {
                    std::vector<double> cumulativeWeights(checksWithJunkWeights.size());
                    std::partial_sum(checksWithJunkWeights.begin(), checksWithJunkWeights.end(),
                                     cumulativeWeights.begin());
                    double random = Ship_Random(0, cumulativeWeights.back());
                    auto it = std::upper_bound(cumulativeWeights.begin(), cumulativeWeights.end(), random);
                    size_t index = std::distance(cumulativeWeights.begin(), it);

                    checkWithJunk = checksWithJunk[index];

                    // Remove the check from the list of checks with junk
                    checksWithJunk.erase(checksWithJunk.begin() + index);
                    checksWithJunkWeights.erase(checksWithJunkWeights.begin() + index);
                }

                SPDLOG_DEBUG("Nothing changed, attempting to replace check: {}",
                             Rando::StaticData::Checks[checkWithJunk].name);
            }

            std::vector<std::pair<RandoItemId, int>> nonJunkItemsThatWeHaveNotTried;
            bool anyNonJunkItemsLeft = false;
            for (size_t i = 0; i < itemPool.size(); i++) {
                if (Rando::StaticData::Items[itemPool[i]].randoItemType != RITYPE_JUNK &&
                    Rando::StaticData::Items[itemPool[i]].randoItemType != RITYPE_HEALTH) {
                    anyNonJunkItemsLeft = true;
                    if (nonJunkItemsThatWeHaveTried.find(itemPool[i]) == nonJunkItemsThatWeHaveTried.end() &&
                        IsItemAllowedAtCheck(itemPool[i], checkWithJunk)) {
                        nonJunkItemsThatWeHaveNotTried.push_back({ itemPool[i], i });
                    }
                }
            }

            if (!anyNonJunkItemsLeft) {
                handleError("No non-junk items left");
            }

            if (nonJunkItemsThatWeHaveNotTried.empty()) {
                SPDLOG_TRACE("Already tried all non-junk items, leaving the last non-junk item in place: {}: {}",
                             Rando::StaticData::Checks[checkWithJunk].name,
                             Rando::StaticData::Items[RANDO_SAVE_CHECKS[checkWithJunk].randoItemId].spoilerName);
                checkWithJunk = RC_UNKNOWN;
                nonJunkItemsThatWeHaveTried.clear();
                continue;
            }

            // Remove item and place it back in the pool
            RandoItemId oldRandoItemId = RANDO_SAVE_CHECKS[checkWithJunk].randoItemId;
            auto& [newRandoItemId, indexInPool] = nonJunkItemsThatWeHaveNotTried[0];

            RANDO_SAVE_CHECKS[checkWithJunk].randoItemId = newRandoItemId;

            SPDLOG_DEBUG("Item Replaced Junk: {}:{}", Rando::StaticData::Checks[checkWithJunk].name,
                         Rando::StaticData::Items[newRandoItemId].spoilerName);

            RemoveItem(oldRandoItemId);
            GiveItem(ConvertItem(newRandoItemId));

            itemPool.erase(itemPool.begin() + indexInPool);
            itemPool.push_back(oldRandoItemId);

            nonJunkItemsThatWeHaveTried.insert(newRandoItemId);
            SPDLOG_TRACE("Attempting to replaced junk item: {}:{}", Rando::StaticData::Checks[checkWithJunk].name,
                         Rando::StaticData::Items[newRandoItemId].spoilerName);
        } else {
            weight++;
            if (checkWithJunk != RC_UNKNOWN) {
                SPDLOG_DEBUG("Successfully Replaced junk item with: {}:{}",
                             Rando::StaticData::Checks[checkWithJunk].name,
                             Rando::StaticData::Items[RANDO_SAVE_CHECKS[checkWithJunk].randoItemId].spoilerName);
            }
            checkWithJunk = RC_UNKNOWN;
            nonJunkItemsThatWeHaveTried.clear();

            // Shuffle the item pool
            if (itemPool.size() > 1) {
                for (size_t i = 0; i < itemPool.size(); i++) {
                    size_t j = Ship_Random(0, itemPool.size());
                    std::swap(itemPool[i], itemPool[j]);
                }
            }
        }
    }

    for (auto& randoCheckId : checksInLogic) {
        copiedSaveContext.save.shipSaveInfo.rando.randoSaveChecks[randoCheckId].randoItemId =
            RANDO_SAVE_CHECKS[randoCheckId].randoItemId;
        copiedSaveContext.save.shipSaveInfo.rando.randoSaveChecks[randoCheckId].shuffled =
            RANDO_SAVE_CHECKS[randoCheckId].shuffled;
    }

    memcpy(&gSaveContext, &copiedSaveContext, sizeof(SaveContext));

    SPDLOG_INFO("Successfully placed all items with Glitchless logic");
}

} // namespace Logic

} // namespace Rando
