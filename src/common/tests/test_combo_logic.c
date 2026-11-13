/**
 * @file test_combo_logic.c
 * @brief ROM-free locks for the combo-logic coordinator (ADR 0010 increment 3,
 *        #645; O4 = composition).
 *
 * Every claim here is driven over TWO SYNTHETIC STUB ENGINES authored in this
 * file, which is the whole reason the engine surface is a registered vtable
 * rather than two sets of fixed `extern "C"` symbols (combo_logic.h says so at
 * the top). A stub world is a bitmask: an item id maps to a bit, a host is
 * reached when its required bits are held, the crossing and the half-goal are
 * each a required mask. That is small enough to reason about exactly and rich
 * enough to carry every property the coordinator claims.
 *
 * Four rows, split by what each one can prove:
 *
 *   combo-logic-engine-surface — the CONTRACT. Registration refuses a bad ABI,
 *   a hole in the vtable, and a snapshot without its restore; un-registration
 *   restores the registry. The GOAL truth table, including triforce-hunt
 *   REFUSING rather than being approximated (answer O10's shared piece count
 *   does not exist). A fill with one engine refuses instead of half-filling, and
 *   a refusal taken BEFORE any attempt reports on the call rather than on the
 *   tables — asserted over FULL tables, because over empty ones "it placed
 *   nothing" is a fact about the fixture. ADR 0002 routing, split into the half
 *   this file's code controls (the `SharedItem` reaching `place` still carries
 *   its foreign origin; the item never reached its own engine's `assumeOwnItem`)
 *   and the half the stub models (the junk cover, which is the two engine lanes'
 *   obligation). And the two premise watchdogs: an engine whose reached set
 *   SHRINKS is reported NON_MONOTONE, an engine that reports change forever is
 *   reported NO_FIXPOINT at the bound — detected, bounded, not looped on.
 *
 *   combo-logic-fixpoint — one ROUND. It terminates in a handful of
 *   alternations; it is order-independent (the same world assembled with the
 *   assumed set and the placements in a different order reports the same
 *   facts); crossing exchange works in BOTH directions in one world; the
 *   ARRIVAL GATE holds (with OoT's crossing closed, MM's hosts are not
 *   candidates and MM's goal does not count); and THE PAIR-LEVEL LOCK WITH
 *   REMOVAL — an OoT-origin progression item hosted only in the MM stub makes
 *   beat-both provable, and removing that host flips it unprovable. Both halves,
 *   because without the removal the lock is theatre. The MM bracket's
 *   re-apply-after-restore is locked the same way: a second round over a
 *   restore that drops the engine's table still proves the goal, which it can
 *   only do because the coordinator put the placements back.
 *
 *   combo-logic-fill — the FILL. Same seed, same placement digest; a different
 *   seed, a different one (the sensitivity control, without which the first half
 *   is satisfied by a constant). The `none` rung places the whole bag with the
 *   proof skipped, where `beatable` on the identical world refuses — and, on the
 *   one world where the two host sources disagree (its only free host is
 *   unreached and never offered), `none` PLACES THERE while `beatable` refuses
 *   with ERR_NO_CANDIDATE, which is what pins `none` to "draw from all empties,
 *   run no round" rather than to a third reachability-gated rung. An engine that
 *   refuses a host it had itself offered aborts the fill AND leaves no row in
 *   the coordinator's table claiming that host. `beatable`
 *   and `beat-either` produce BYTE-IDENTICAL placements on a world where both
 *   halves prove — the no-bias lock, since a fill that ever short-circuited on
 *   one half would diverge there — and on a world where MM's half cannot prove,
 *   `beat-either` succeeds while `beat-both` refuses, with MM's hosts still
 *   receiving items (the permitted unbeatable half is not starved). The
 *   `all-reachable` rung refuses a world whose placed host is never reached
 *   where `beatable` accepts it.
 *
 *   combo-logic-contract-edges — the contract's EDGES, which the three rows above
 *   cannot reach because their worlds are six hosts per side and their stubs never
 *   refuse a call. An engine offering 2300 hosts is ACCEPTED (the enumeration cap
 *   is a game's check id-space, not the bag) and one host past
 *   RSBS_COMBO_LOGIC_HOST_CAP is REFUSED rather than truncated. Both part-opened
 *   teardowns of the bracket's ownership rule: a `beginQuery` that refuses with no
 *   snapshot pair still gets `endQuery` and no `restore`, and one that refuses
 *   AFTER a successful `snapshot` gets `restore`, a full re-apply of its whole
 *   table, and `endQuery` — with a harsh restore, so the re-apply is measured and
 *   not assumed. A refused `snapshot` gets none of the three. And `RunFill` takes
 *   both tables EMPTY at every attempt, engines included, so an authored partial
 *   world is discarded rather than extended.
 *
 *   combo-logic-forbidden-host — the round's FORBIDDEN-HOST input (ABI 6, epic
 *   #575 H4), over one world that holds an own-origin and a cross-origin item on
 *   each side. A forbidden host's item is never harvested (own origin, withheld
 *   by its engine through `withholdHost`) and never exchanged (cross origin,
 *   withheld by the coordinator), the half-goal that item carried falls, the host
 *   itself stays reached, and the call is routed to the host's own engine only.
 *   An empty forbidden set, and a forbidden host that holds nothing, change no
 *   round: the result is byte-identical to the round without the field. Nothing
 *   leaks into the next round. A malformed set, or a host of an engine with no
 *   `withholdHost`, is refused before any bracket opens.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE and therefore
 * compiled as C++, like every other file in this directory; every symbol under
 * test is C-linkage through combo_logic.h.
 */

#include "../combo_logic.h"
#include "../combo_single_bag.h" // Combo_SingleBag_StatusIsWorldDeadEnd: which refusals may climb the ladder
#include "../context.h"
#include "../shared_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>

#define CL_ASSERT(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            return TEST_FAIL;                                               \
        }                                                                   \
    } while (0)

namespace {

// ============================================================================
// The stub engine
// ============================================================================

const int kClMaxHosts = 16;
/** Placements one stub engine can STORE: a synthetic world (below) can offer and
 *  fill far more hosts than the authored ones, so the store is wider (P6 places
 *  more than RSBS_SHARED_ITEM_CAP crossings on one side). */
const int kClMaxPlaced = 160;
const int kClMaxItems = 8;

/**
 * First id of each side's SYNTHETIC host pool (`ClEngine.syntheticHosts`).
 *
 * ONE BASE PER GAME, AND FAR APART, for the same reason the authored ids below are
 * disjoint: the fixture's trap is that an OoT id and an MM id can never be mistaken
 * for one another, so a misrouted host is a DETECTED ADR 0002 violation rather than
 * a plausible-looking wrong answer (the #356 class). A single shared base would have
 * broken exactly that in the one world that offers thousands of hosts —
 * `ClBuildWideHostWorld(2300, 4)` would have had both sides offering 1000..1003.
 *
 * The widest pool either side is built with here is RSBS_COMBO_LOGIC_HOST_CAP + 1
 * (4097), so OoT occupies at most 1000..5096 and MM at most 20000..24096: two
 * disjoint runs, both inside the coordinator's 0..65535 occupancy id-space, and both
 * disjoint from every authored id.
 */
const uint16_t kClSyntheticHostBaseOoT = 1000;
const uint16_t kClSyntheticHostBaseMM = 20000;

// OoT-side ids. Disjoint from the MM ids below ON PURPOSE: that disjointness is
// what turns a misrouted id into a detected ADR 0002 violation instead of a
// plausible-looking wrong answer (the #356 class).
const uint16_t kOotSword = 101;
const uint16_t kOotHook = 102;
const uint16_t kOotLens = 103;
const uint16_t kOotBoots = 104;
const uint16_t kOotJunk = 190; // the junk cover an OoT host gets for a foreign item

const uint16_t kMmOcarina = 201;
const uint16_t kMmBow = 202;
const uint16_t kMmRemains = 203;
const uint16_t kMmMask = 204;
const uint16_t kMmJunk = 290;

enum ClFault {
    CL_FAULT_NONE = 0,
    /** The reached set collapses on the second expand of a round, and the expand
     *  still reports "changed" — a monotone observable falling, which is the
     *  violation ERR_NON_MONOTONE names. */
    CL_FAULT_SHRINK = 1,
    /** `expand` reports change forever with nothing growing — the lie that would
     *  hang world generation, and which ERR_NO_FIXPOINT bounds. */
    CL_FAULT_ALWAYS_CHANGED = 2
};

struct ClEngine {
    uint8_t game;

    // --- the world -------------------------------------------------------
    int hostCount;
    uint16_t hostId[kClMaxHosts]; // ASCENDING: the order contract
    uint32_t hostRequires[kClMaxHosts];
    uint32_t alwaysOffer; // host indices offered even when unreached
    int itemCount;
    uint16_t itemId[kClMaxItems]; // index k <-> bit (1u << k)
    uint32_t crossingRequires;
    uint32_t goalRequires;
    /** COUNTED requirements (the multiplicity ruling). A host `i` with
     *  `hostNeedCount[i] > 0` is reached only while this engine holds at least
     *  that many COPIES of `hostNeedItem[i]`; the goal likewise. Zero = no count
     *  term, which is every world authored before ABI 3. */
    uint16_t hostNeedItem[kClMaxHosts];
    uint8_t hostNeedCount[kClMaxHosts];
    uint16_t goalNeedItem;
    uint8_t goalNeedCount;
    uint16_t junkId;
    bool overReport;             // offer hosts it has already placed on
    bool restoreDropsPlacements; // a harsh restore: the table comes back empty
    bool withSnapshot;
    /** Refuse the next NEW placement (an idempotent re-apply still succeeds).
     *  Models an engine that rejects a host it had itself offered — the branch
     *  whose table entry the coordinator must take back out. */
    bool refuseNextNewPlace;
    /**
     * When nonzero, this engine's host enumeration is SYNTHETIC: both
     * `reachedEmptyHosts` and `allEmptyHosts` offer `syntheticHosts` ids
     * (`ClSyntheticBase(this engine) + i`, ascending — a different base per game, so
     * the two sides' synthetic pools stay disjoint) and `checkReached` answers yes
     * for all of them, in place of the authored table.
     *
     * It exists because the quantity the coordinator's host scratch must hold is a
     * GAME'S CHECK POOL — OoT's `RandomizerCheck` has 2528 rows and MM's 2258 —
     * and `kClMaxHosts` authored hosts cannot express a list that size. Only a
     * stub that offers thousands can tell a scratch sized by the placement cap
     * (1024) from one sized by the id space.
     */
    int syntheticHosts;
    /** `beginQuery` performs its detach and THEN refuses. The point is the
     *  ordering: the coordinator owes `endQuery` for a bracket that was opened
     *  far enough to detach, whatever `beginQuery` returned. */
    bool refuseBeginQuery;
    /** `snapshot` refuses, having captured nothing. That side must then get no
     *  `beginQuery`, no `endQuery` and no `restore`. */
    bool refuseSnapshot;
    /** ABI 8: install `allLocations`, listing every authored host EXCEPT the host
     *  indices in `notLocation` — the stub's CURATED EXCLUSIONS (a host its world
     *  makes unreachable by construction, left out of the All Locations obligation).
     *  Clear, the vtable entry is NULL and the coordinator's fallback (the host
     *  table) stands in, which is every world authored before ABI 8. */
    bool withAllLocations;
    uint32_t notLocation;
    /** ABI 9 (#1502, THE HOST GROUPS): install `hostGroups`, answering
     *  `hostGroup[i]` for authored host i (0 for any other id). Clear, the entry
     *  is NULL, which is every world authored before ABI 9. */
    bool withHostGroups;
    uint32_t hostGroup[kClMaxHosts];
    int hostGroupCalls;
    /** ABI 11 (#1542, THE COMBO-WIDE GROUPS): install `wideGroups`, answering
     *  `wideDeclared`. Clear, the entry is NULL, which is every world authored
     *  before ABI 11. */
    bool withWideGroups;
    uint32_t wideDeclared;
    int wideGroupCalls;
    /** #1542: install `hostAcceptsForeign`, refusing authored host i while bit i of
     *  `foreignRefused` is set. Clear, the entry is NULL (every host accepts). */
    bool withAcceptsForeign;
    uint32_t foreignRefused;
    /** ABI 10 (#1169 XW9, THE TRAVEL FACTS): install `travelFacts` and (unless
     *  `noAcceptTravel`) `acceptTravelFacts`. Clear, both entries are NULL, which is
     *  every world authored before ABI 10. Fact bit b (RSBS_COMBO_TRAVEL_* space) is
     *  reported while it is in `factEnabled` and `have` covers `factRequires[b]`;
     *  `factReportExtra` is reported always (the own-direction trap); with
     *  `factDropAfterFirst`, every call after the first reports nothing (a fact that
     *  disappears inside a round). Host i is reached only while the ACCEPTED facts
     *  cover `hostFactRequires[i]` (0 = no fact term). */
    bool withTravelFacts;
    bool noAcceptTravel;
    uint32_t factEnabled;
    uint32_t factRequires[8];
    uint32_t factReportExtra;
    bool factDropAfterFirst;
    uint32_t hostFactRequires[kClMaxHosts];
    /** #1169 XW15 (THE FIXED SHARED CREDIT): authored host i's FIXED own content,
     *  the engine's native item there (never a table row), harvested like an own
     *  placement while the host has none. 0 = no fixed content, which is every
     *  world authored before XW15. */
    uint16_t fixedItem[kClMaxHosts];
    ClFault fault;

    // --- live state ------------------------------------------------------
    uint32_t have;
    uint32_t reached;
    /** COPIES held this round, per item bit — one per `assumeOwnItem` call and
     *  one per harvested own-origin host. `have` stays the "at least one" view. */
    uint8_t haveCount[kClMaxItems];
    /** Hosts whose own-origin placement was harvested THIS round: the contract's
     *  "once per placed host per round". */
    uint32_t harvested;
    /** Hosts this round WITHHOLDS (ABI 6, `withholdHost`): reached as ever, never
     *  harvested. Cleared by `beginQuery` and `endQuery`, so it is one round's. */
    uint32_t withheld;
    /** `withholdHost` calls since the engine was reset, and how many named a host
     *  this engine does not own — a misrouted host (ADR 0002). */
    int withholdCalls;
    int withholdUnknown;
    /** `assumeOwnItem` calls per item bit since the engine was reset — what the
     *  one-call-per-copy locks count. Never reset by a round. */
    int assumeCalls[kClMaxItems];
    bool inQuery;
    /** ABI 10: the other game's facts this round accepted (reset by `beginQuery`
     *  and `endQuery`), the calls since the engine was reset, and the trap for an
     *  accept that is not a superset of the previous one. */
    uint32_t accepted;
    int travelCalls;
    int acceptCalls;
    bool sawAcceptShrink;
    /** MODELS OoT'S COUPLING. `beginQuery` sets it (OoT's `Logic::Reset(true)`
     *  re-points `mSaveContext` at a heap copy) and `endQuery` is the only thing
     *  that clears it (`mSaveContext` back to `&gSaveContext`, audit §4.4). An
     *  engine left `detached` after a round is audit §1.8's hazard: the next
     *  in-game progressive give resolves against a simulated save. */
    bool detached;
    int expands;
    int beginCalls;
    int endCalls;
    int snapshots;
    int restores;
    int placeCalls;
    uint32_t snapHave;
    uint32_t snapReached;
    uint8_t snapCount[kClMaxItems];
    uint32_t snapHarvested;
    int snapPlaced;

    int placedCount;
    uint16_t placedHost[kClMaxPlaced];
    SharedItem placedItem[kClMaxPlaced]; // what the engine STORED (a cover, if foreign)
    /** What the COORDINATOR handed to `place`, verbatim. The cover above is this
     *  stub's own doing and proves nothing about the coordinator; this array is
     *  the half the coordinator controls, so it is what the ADR 0002 routing
     *  assertions read. */
    SharedItem placedRaw[kClMaxPlaced];
    bool placedIsCover[kClMaxPlaced];

    // --- contract traps --------------------------------------------------
    bool sawForeignAssume;     // an id this engine's table does not name
    bool sawForeignInOwnTable; // a foreign-origin SharedItem stored verbatim
    bool sawQueryOutsideRound; // a query call outside begin/end
    bool sawDoubleBegin;
};

ClEngine gClOoT;
ClEngine gClMM;
ComboLogicEngine gClVtOoT;
ComboLogicEngine gClVtMM;

int ClBitIndex(const ClEngine* e, uint16_t id) {
    for (int k = 0; k < e->itemCount; ++k) {
        if (e->itemId[k] == id) {
            return k;
        }
    }
    return -1;
}

int ClHostIndex(const ClEngine* e, uint16_t host) {
    for (int i = 0; i < e->hostCount; ++i) {
        if (e->hostId[i] == host) {
            return i;
        }
    }
    return -1;
}

int ClOwnPlacementIndex(const ClEngine* e, uint16_t host) {
    for (int p = 0; p < e->placedCount; ++p) {
        if (e->placedHost[p] == host) {
            return p;
        }
    }
    return -1;
}

/** Does this engine hold `count` copies of `id`? A zero count is no term. */
bool ClHoldsCopies(const ClEngine* e, uint16_t id, uint8_t count) {
    if (count == 0) {
        return true;
    }
    const int k = ClBitIndex(e, id);
    return k >= 0 && e->haveCount[k] >= count;
}

bool ClHostOpen(const ClEngine* e, int i) {
    return (e->hostRequires[i] & e->have) == e->hostRequires[i] &&
           (e->hostFactRequires[i] & e->accepted) == e->hostFactRequires[i] &&
           ClHoldsCopies(e, e->hostNeedItem[i], e->hostNeedCount[i]);
}

/** The stub's own local fixpoint: which hosts are reached, and which of its own
 *  items it harvests from them. The own-origin harvest lives HERE and not in the
 *  coordinator on purpose — picking up your own item from your own check is the
 *  engine's own search doing what OoT's ReachabilitySearch already does. It is
 *  ONCE PER HOST PER ROUND (`harvested`), so two hosts holding one id are two
 *  copies and no host is ever counted twice. */
void ClRecompute(ClEngine* e) {
    for (;;) {
        uint32_t nextReached = 0;
        for (int i = 0; i < e->hostCount; ++i) {
            if (ClHostOpen(e, i)) {
                nextReached |= (1u << i);
            }
        }
        bool harvestedAny = false;
        for (int i = 0; i < e->hostCount; ++i) {
            if ((nextReached & (1u << i)) == 0u || (e->harvested & (1u << i)) != 0u ||
                (e->withheld & (1u << i)) != 0u) {
                continue;
            }
            const int p = ClOwnPlacementIndex(e, e->hostId[i]);
            // An own placement first; with none, the host's FIXED content (XW15).
            const uint16_t own = (p >= 0) ? (e->placedIsCover[p] ? (uint16_t)0 : e->placedItem[p].id) : e->fixedItem[i];
            if (own == 0) {
                continue;
            }
            const int k = ClBitIndex(e, own);
            if (k >= 0) {
                e->have |= (1u << k);
                if (e->haveCount[k] < 255) {
                    e->haveCount[k]++;
                }
                e->harvested |= (1u << i);
                harvestedAny = true;
            }
        }
        if (nextReached == e->reached && !harvestedAny) {
            return;
        }
        e->reached = nextReached;
    }
}

/** Total copies held, for the `changed` answer. */
int ClCopiesHeld(const ClEngine* e) {
    int total = 0;
    for (int k = 0; k < e->itemCount; ++k) {
        total += e->haveCount[k];
    }
    return total;
}

int ClBeginQuery(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (e->inQuery) {
        e->sawDoubleBegin = true;
    }
    e->beginCalls++;
    // THE DETACH FIRST, then the step that can fail — OoT's engine order exactly
    // (`Logic::Reset(true)` runs before anything else in its `beginQuery`). A
    // refusal from here on has already changed the engine's coupling, so the only
    // thing that can put it back is `endQuery`.
    e->detached = true;
    if (e->refuseBeginQuery) {
        return 0;
    }
    e->inQuery = true;
    e->expands = 0;
    // The world's STARTING inventory. This is the call whose omission would make
    // every round after the first answer a question about a richer world than it
    // was asked about (combo_logic.h on beginQuery).
    e->have = 0u;
    e->reached = 0u;
    memset(e->haveCount, 0, sizeof(e->haveCount));
    e->harvested = 0u;
    e->withheld = 0u;
    e->accepted = 0u;
    return 1;
}

void ClEndQuery(void* self) {
    ClEngine* e = (ClEngine*)self;
    e->endCalls++;
    e->inQuery = false;
    e->withheld = 0u;
    e->accepted = 0u;
    // THE INVERSE OF THE DETACH, and the only one there is. Unconditional on
    // purpose: the coordinator may reach here on a round that never opened (a
    // `beginQuery` that refused after detaching), and re-coupling is exactly what
    // that case needs.
    e->detached = false;
}

void ClAssumeOwnItem(void* self, uint16_t ownItemId) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    const int k = ClBitIndex(e, ownItemId);
    if (k < 0) {
        // ADR 0002: an id this engine's id-space does not name got routed here.
        e->sawForeignAssume = true;
        return;
    }
    e->have |= (1u << k); // may only ADD
    // ONE CALL IS ONE COPY (ABI 3): counted, never de-duplicated.
    if (e->haveCount[k] < 255) {
        e->haveCount[k]++;
    }
    e->assumeCalls[k]++;
}

int ClExpand(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    e->expands++;
    const uint32_t haveBefore = e->have;
    const uint32_t reachedBefore = e->reached;
    const int copiesBefore = ClCopiesHeld(e);
    ClRecompute(e);
    if (e->fault == CL_FAULT_SHRINK && e->expands >= 2) {
        e->reached = 0u;
        e->have = 0u;
        return 1;
    }
    if (e->fault == CL_FAULT_ALWAYS_CHANGED) {
        return 1;
    }
    return (e->have != haveBefore || e->reached != reachedBefore || ClCopiesHeld(e) != copiesBefore) ? 1 : 0;
}

/** ABI 6: this round never harvests `hostCheck`'s contents. */
void ClWithholdHost(void* self, uint16_t hostCheck) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    e->withholdCalls++;
    const int i = ClHostIndex(e, hostCheck);
    if (i < 0) {
        e->withholdUnknown++;
        return;
    }
    e->withheld |= (1u << i);
}

int ClCrossingOpen(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    return ((e->crossingRequires & e->have) == e->crossingRequires) ? 1 : 0;
}

/** This engine's synthetic pool base. Per GAME, never shared — see the two
 *  constants' rationale at the top of the fixture. */
uint16_t ClSyntheticBase(const ClEngine* e) {
    return (e->game == (uint8_t)GAME_OOT) ? kClSyntheticHostBaseOoT : kClSyntheticHostBaseMM;
}

/** Is `host` one of this engine's synthetic ids? An id from the OTHER side's
 *  synthetic pool answers NO, which is the point: a misrouted synthetic host
 *  reads as "not a host of mine" instead of as a plausible one. */
bool ClIsSyntheticHost(const ClEngine* e, uint16_t host) {
    const uint16_t base = ClSyntheticBase(e);
    return e->syntheticHosts > 0 && host >= base && (int)(host - base) < e->syntheticHosts;
}

/**
 * The synthetic enumeration shared by both host calls: ascending ids (the order
 * contract), every one of them offered, and the TOTAL returned even when it
 * exceeds `cap` so the coordinator can tell truncation from exhaustion.
 *
 * It deliberately does not filter this engine's own placements. Occupancy has
 * exactly one authority and it is the coordinator's tables (combo_logic.h), so
 * over-reporting is free — and here it keeps the offered COUNT a fixed property
 * of the world, which is the whole quantity under test.
 */
int ClEnumerateSynthetic(const ClEngine* e, uint16_t* out, int cap) {
    const uint16_t base = ClSyntheticBase(e);
    for (int i = 0; i < e->syntheticHosts; ++i) {
        if (out != NULL && i < cap) {
            out[i] = (uint16_t)(base + i);
        }
    }
    return e->syntheticHosts;
}

int ClCheckReached(void* self, uint16_t hostCheck) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    if (ClIsSyntheticHost(e, hostCheck)) {
        return 1;
    }
    const int i = ClHostIndex(e, hostCheck);
    if (i < 0) {
        return 0;
    }
    return ((e->reached & (1u << i)) != 0u) ? 1 : 0;
}

int ClReachedEmptyHosts(void* self, uint16_t* out, int cap) {
    ClEngine* e = (ClEngine*)self;
    int total = 0;
    if (e->syntheticHosts > 0) {
        return ClEnumerateSynthetic(e, out, cap);
    }
    for (int i = 0; i < e->hostCount; ++i) {
        const bool reached = (e->reached & (1u << i)) != 0u;
        const bool forced = (e->alwaysOffer & (1u << i)) != 0u;
        if (!reached && !forced) {
            continue;
        }
        if (!e->overReport && ClOwnPlacementIndex(e, e->hostId[i]) >= 0) {
            continue;
        }
        if (out != NULL && total < cap) {
            out[total] = e->hostId[i]; // ascending, by construction
        }
        total++;
    }
    return total;
}

/** Every host not in this engine's own placement table, REACHED OR NOT, and
 *  legal outside a query bracket — the `none` rung's host source. Deliberately
 *  does NOT touch `reached`, and deliberately does NOT set
 *  `sawQueryOutsideRound`: this is the one query the contract says may be asked
 *  with no round open. */
int ClAllEmptyHosts(void* self, uint16_t* out, int cap) {
    ClEngine* e = (ClEngine*)self;
    int total = 0;
    if (e->syntheticHosts > 0) {
        return ClEnumerateSynthetic(e, out, cap);
    }
    for (int i = 0; i < e->hostCount; ++i) {
        if (!e->overReport && ClOwnPlacementIndex(e, e->hostId[i]) >= 0) {
            continue;
        }
        if (out != NULL && total < cap) {
            out[total] = e->hostId[i]; // ascending, by construction
        }
        total++;
    }
    return total;
}

int ClGoalReached(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    return ((e->goalRequires & e->have) == e->goalRequires && ClHoldsCopies(e, e->goalNeedItem, e->goalNeedCount)) ? 1
                                                                                                                   : 0;
}

int ClPlace(void* self, uint16_t hostCheck, SharedItem item) {
    ClEngine* e = (ClEngine*)self;
    e->placeCalls++;

    const bool foreign = (item.originGame != e->game);
    const int existing = ClOwnPlacementIndex(e, hostCheck);
    if (existing >= 0) {
        // IDEMPOTENT for the same (host, item): the coordinator re-applies the
        // whole table after every restore.
        if (e->placedIsCover[existing] == foreign && (foreign || e->placedItem[existing].id == item.id)) {
            return 1;
        }
        return 0; // a different item on an assigned host is a coordinator defect
    }
    if (e->refuseNextNewPlace) {
        // An engine refusing a host it had itself offered. One-shot, so the
        // coordinator's re-apply of an already-accepted placement is unaffected.
        e->refuseNextNewPlace = false;
        return 0;
    }
    if (e->placedCount >= kClMaxPlaced) {
        return 0;
    }
    e->placedHost[e->placedCount] = hostCheck;
    e->placedRaw[e->placedCount] = item; // exactly what the coordinator passed
    e->placedIsCover[e->placedCount] = foreign;
    if (foreign) {
        // JUNK COVER. The engine's own table gets a legal LOCAL item; the
        // foreign identity stays in the coordinator's table (ADR 0002). If this
        // stored `item` verbatim, the trap below would fire.
        SharedItem cover;
        cover.originGame = e->game;
        cover.flags = 0u;
        cover.id = e->junkId;
        e->placedItem[e->placedCount] = cover;
    } else {
        e->placedItem[e->placedCount] = item;
    }
    if (e->placedItem[e->placedCount].originGame != e->game) {
        e->sawForeignInOwnTable = true;
    }
    e->placedCount++;
    // NOT granted: placing is not holding.
    return 1;
}

void ClClearPlacements(void* self) {
    ClEngine* e = (ClEngine*)self;
    // Only what the coordinator put here. A real engine drops its
    // coordinator-assigned shuffled checks and leaves its graph and its own
    // restricted-pool placements alone.
    e->placedCount = 0;
    e->snapPlaced = 0;
}

int ClSnapshot(void* self) {
    ClEngine* e = (ClEngine*)self;
    e->snapshots++;
    if (e->refuseSnapshot) {
        // NOTHING WAS CAPTURED. So the coordinator must not restore this side, and
        // must not query it either — a query under a failed snapshot writes MM's
        // live save with no way back (combo_logic.h on the pair).
        return 0;
    }
    e->snapHave = e->have;
    e->snapReached = e->reached;
    memcpy(e->snapCount, e->haveCount, sizeof(e->snapCount));
    e->snapHarvested = e->harvested;
    e->snapPlaced = e->placedCount;
    return 1;
}

void ClRestore(void* self) {
    ClEngine* e = (ClEngine*)self;
    e->restores++;
    e->have = e->snapHave;
    e->reached = e->snapReached;
    memcpy(e->haveCount, e->snapCount, sizeof(e->haveCount));
    e->harvested = e->snapHarvested;
    // The harsh variant models a restore whose captured blob does not carry the
    // placement writes. The coordinator's re-apply is what makes the coordinator
    // correct against it either way (audit amendment 1).
    e->placedCount = e->restoreDropsPlacements ? 0 : e->snapPlaced;
}

/** ABI 8's `allLocations`: every authored host not in `notLocation`, ascending (the
 *  order contract), the TOTAL returned. Legal outside a round, so it touches no
 *  round state and sets no trap. */
int ClAllLocations(void* self, uint16_t* out, int cap) {
    ClEngine* e = (ClEngine*)self;
    int total = 0;
    for (int i = 0; i < e->hostCount; ++i) {
        if ((e->notLocation & (1u << i)) != 0u) {
            continue;
        }
        if (out != NULL && total < cap) {
            out[total] = e->hostId[i];
        }
        total++;
    }
    return total;
}

/** ABI 9's `hostGroups`: the authored host's group bits. Legal outside a round,
 *  so it sets no trap; counted, so a lock can see it was (or was not) asked. */
uint32_t ClHostGroups(void* self, uint16_t hostCheck) {
    ClEngine* e = (ClEngine*)self;
    e->hostGroupCalls++;
    const int i = ClHostIndex(e, hostCheck);
    return (i >= 0) ? e->hostGroup[i] : 0u;
}

/** ABI 11's `wideGroups`: the declared wide bits. Counted, so a lock can see a
 *  mask-free fill never asks. */
uint32_t ClWideGroups(void* self) {
    ClEngine* e = (ClEngine*)self;
    e->wideGroupCalls++;
    return e->wideDeclared;
}

/** #1542: `hostAcceptsForeign`, refusing the hosts in `foreignRefused`. */
int ClHostAcceptsForeign(void* self, uint16_t hostCheck) {
    ClEngine* e = (ClEngine*)self;
    const int i = ClHostIndex(e, hostCheck);
    return (i >= 0 && (e->foreignRefused & (1u << i)) != 0u) ? 0 : 1;
}

/** ABI 10: the facts this stub can play now (see `withTravelFacts`). */
uint32_t ClTravelFacts(void* self) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    e->travelCalls++;
    if (e->factDropAfterFirst && e->travelCalls > 1) {
        return 0u;
    }
    uint32_t facts = e->factReportExtra;
    for (int b = 0; b < 8; ++b) {
        const uint32_t bit = 1u << b;
        if ((e->factEnabled & bit) != 0u && (e->factRequires[b] & e->have) == e->factRequires[b]) {
            facts |= bit;
        }
    }
    return facts;
}

/** ABI 10: the other game's gated facts for the rest of the round. */
void ClAcceptTravelFacts(void* self, uint32_t facts) {
    ClEngine* e = (ClEngine*)self;
    if (!e->inQuery) {
        e->sawQueryOutsideRound = true;
    }
    if ((facts & e->accepted) != e->accepted) {
        e->sawAcceptShrink = true;
    }
    e->acceptCalls++;
    e->accepted = facts;
}

void ClFillVtable(ComboLogicEngine* vt, ClEngine* e) {
    memset(vt, 0, sizeof(*vt));
    vt->abiVersion = RSBS_COMBO_LOGIC_ENGINE_ABI;
    vt->self = e;
    vt->beginQuery = ClBeginQuery;
    vt->assumeOwnItem = ClAssumeOwnItem;
    vt->expand = ClExpand;
    vt->crossingOpen = ClCrossingOpen;
    vt->checkReached = ClCheckReached;
    vt->reachedEmptyHosts = ClReachedEmptyHosts;
    vt->allEmptyHosts = ClAllEmptyHosts;
    vt->goalReached = ClGoalReached;
    vt->place = ClPlace;
    vt->clearPlacements = ClClearPlacements;
    vt->endQuery = ClEndQuery;
    vt->withholdHost = ClWithholdHost;
    if (e->withAllLocations) {
        vt->allLocations = ClAllLocations;
    }
    if (e->withHostGroups) {
        vt->hostGroups = ClHostGroups;
    }
    if (e->withWideGroups) {
        vt->wideGroups = ClWideGroups;
    }
    if (e->withAcceptsForeign) {
        vt->hostAcceptsForeign = ClHostAcceptsForeign;
    }
    if (e->withTravelFacts) {
        vt->travelFacts = ClTravelFacts;
        if (!e->noAcceptTravel) {
            vt->acceptTravelFacts = ClAcceptTravelFacts;
        }
    }
    if (e->withSnapshot) {
        vt->snapshot = ClSnapshot;
        vt->restore = ClRestore;
    }
}

void ClResetEngine(ClEngine* e, uint8_t game) {
    memset(e, 0, sizeof(*e));
    e->game = game;
    e->junkId = (game == (uint8_t)GAME_OOT) ? kOotJunk : kMmJunk;
}

// `reqMask`, not `requires`: the latter is a C++20 keyword and this file is
// compiled as C++ (the linkage note in the file header).
void ClAddHost(ClEngine* e, uint16_t id, uint32_t reqMask) {
    e->hostId[e->hostCount] = id;
    e->hostRequires[e->hostCount] = reqMask;
    e->hostCount++;
}

void ClAddItem(ClEngine* e, uint16_t id) {
    e->itemId[e->itemCount++] = id;
}

/** Item bit by id, for authoring requirement masks in the scenarios below. */
uint32_t ClBit(const ClEngine* e, uint16_t id) {
    const int k = ClBitIndex(e, id);
    return (k >= 0) ? (1u << k) : 0u;
}

SharedItem ClItem(uint8_t origin, uint16_t id) {
    SharedItem s;
    s.originGame = origin;
    s.flags = 0u;
    s.id = id;
    return s;
}

ComboLogicBagItem ClBagItem(uint8_t origin, uint16_t id, uint16_t itemClass) {
    ComboLogicBagItem b;
    b.item = ClItem(origin, id);
    b.itemClass = itemClass;
    b.bagFlags = 0u; // a REQUIRED copy
    return b;
}

/** A SURPLUS copy (THE BAG MODEL, shape 2). */
ComboLogicBagItem ClSurplusItem(uint8_t origin, uint16_t id, uint16_t itemClass) {
    ComboLogicBagItem b = ClBagItem(origin, id, itemClass);
    b.bagFlags = RSBS_COMBO_BAG_SURPLUS;
    return b;
}

/** Register both stubs and clear the tables. Call after every world edit: the
 *  vtable carries the `self` pointer and the snapshot pair. */
void ClInstall() {
    ClFillVtable(&gClVtOoT, &gClOoT);
    ClFillVtable(&gClVtMM, &gClMM);
    Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT);
    Combo_Logic_RegisterEngine(GAME_MM, &gClVtMM);
    Combo_Logic_ResetPlacements();
}

void ClUninstall() {
    // Reset BEFORE un-registering, so the engines are told to forget too.
    Combo_Logic_ResetPlacements();
    Combo_Logic_RegisterEngine(GAME_OOT, NULL);
    Combo_Logic_RegisterEngine(GAME_MM, NULL);
}

/** True when neither stub saw a contract violation — the ADR 0002 routing trap
 *  included. Checked at the end of every scenario. */
bool ClContractClean() {
    const ClEngine* both[2] = { &gClOoT, &gClMM };
    for (int i = 0; i < 2; ++i) {
        const ClEngine* e = both[i];
        if (e->sawForeignAssume || e->sawForeignInOwnTable || e->sawQueryOutsideRound || e->sawDoubleBegin) {
            printf("[TEST]   %s stub: foreignAssume=%d foreignInTable=%d outsideRound=%d doubleBegin=%d\n",
                   Game_ToString((GameId)e->game), (int)e->sawForeignAssume, (int)e->sawForeignInOwnTable,
                   (int)e->sawQueryOutsideRound, (int)e->sawDoubleBegin);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Worlds
// ============================================================================

/**
 * THE CROSSING WORLD. One OoT host and one MM host, each hosting the OTHER
 * game's progression item, so one round has to exchange in BOTH directions:
 *   OoT host 10 (sphere zero) holds MM's Ocarina  -> unlocks MM host 21
 *   MM  host 20 (sphere zero) holds OoT's Lens    -> proves OoT's goal
 * OoT's goal needs the Lens; MM's goal needs the Ocarina.
 */
void ClBuildCrossingWorld(uint32_t ootCrossingItemMask) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, ClBit(&gClOoT, kOotSword));
    gClOoT.crossingRequires = ootCrossingItemMask;
    gClOoT.goalRequires = ClBit(&gClOoT, kOotLens);

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, ClBit(&gClMM, kMmOcarina));
    gClMM.crossingRequires = 0u; // unconditional from MM's root (audit §4.5)
    gClMM.goalRequires = ClBit(&gClMM, kMmOcarina);
    gClMM.overReport = true; // exercise the coordinator's own occupancy filter
    ClInstall();
}

/** A six-host-per-side world where both halves prove, for the fill rows; either
 *  half can be made unprovable by its own parameters (a goal item in no bag and
 *  no host). */
void ClBuildFillWorldHalves(bool ootHalfProvable, bool mmHalfProvable) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddItem(&gClOoT, kOotBoots);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, 0u);
    ClAddHost(&gClOoT, 12, 0u);
    ClAddHost(&gClOoT, 13, ClBit(&gClOoT, kOotSword));
    ClAddHost(&gClOoT, 14, ClBit(&gClOoT, kOotHook));
    ClAddHost(&gClOoT, 15, 0u);
    gClOoT.crossingRequires = 0u;
    // kOotBoots is in no fill bag and on no host: the OoT half's own parameters
    // make it unbeatable, the mirror of the MM variant below.
    gClOoT.goalRequires = ootHalfProvable ? ClBit(&gClOoT, kOotLens) : ClBit(&gClOoT, kOotBoots);

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddItem(&gClMM, kMmMask);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, 0u);
    ClAddHost(&gClMM, 22, 0u);
    ClAddHost(&gClMM, 23, ClBit(&gClMM, kMmOcarina));
    ClAddHost(&gClMM, 24, ClBit(&gClMM, kMmBow));
    ClAddHost(&gClMM, 25, 0u);
    gClMM.crossingRequires = 0u;
    // The unprovable variant needs an item that is in NO bag and in NO host, so
    // the half is unbeatable by its own authored parameters — exactly the
    // configuration answer O1 says `beat-either` must permit.
    gClMM.goalRequires = mmHalfProvable ? ClBit(&gClMM, kMmRemains) : ClBit(&gClMM, kMmMask);
    ClInstall();
}

void ClBuildFillWorld(bool mmHalfProvable) {
    ClBuildFillWorldHalves(true, mmHalfProvable);
}

/**
 * A world whose OoT engine OFFERS a host it never reaches (`alwaysOffer` over a
 * requirement no item in the world can meet), and whose two halves both prove
 * unconditionally. `beatable` cannot see the unreached host; `all-reachable`
 * must refuse it. MM has no hosts, so the single bag item has exactly one place
 * to go and the scenario is deterministic rather than probable.
 */
void ClBuildOfferUnreachedWorld() {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddHost(&gClOoT, 10, 1u << 7); // a requirement no item in this world can meet
    gClOoT.alwaysOffer = 1u << 0;    // ... and it is offered anyway
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    ClInstall();
}

/**
 * A world whose ONLY free host is UNREACHED and NOT offered as a candidate: OoT
 * host 10 requires a bit no item in the world can supply, `alwaysOffer` is
 * clear, and MM has no hosts at all. So `reachedEmptyHosts` never names host 10
 * while `allEmptyHosts` always does — which is exactly the pair of worlds the
 * two host sources disagree about, and therefore the world that decides what
 * `RSBS_COMBO_RUNG_NONE` means.
 */
void ClBuildUnreachedOnlyWorld() {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddHost(&gClOoT, 10, 1u << 7); // a requirement no item in this world meets
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    ClInstall();
}

/**
 * A world whose engines OFFER many more hosts than the placement cap, both halves
 * proving unconditionally, nothing else going on.
 *
 * It is about ONE quantity: the size of the list an engine hands back from
 * `reachedEmptyHosts` / `allEmptyHosts`. That is bounded by the game's check pool
 * (OoT 2528 `RandomizerCheck` rows, MM 2258), and it has nothing to do with how
 * many bag items a side can RECEIVE. A coordinator that conflated the two refuses
 * this world outright.
 *
 * The two sides' pools are DISJOINT here as everywhere else in the fixture: OoT's
 * runs from kClSyntheticHostBaseOoT and MM's from kClSyntheticHostBaseMM, so the
 * ADR 0002 routing trap still has teeth in a world of thousands of hosts.
 */
void ClBuildWideHostWorld(int ootHosts, int mmHosts) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    gClOoT.syntheticHosts = ootHosts;
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    gClMM.syntheticHosts = mmHosts;
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    ClInstall();
}

/** THE SHARED-QUANTITY CREDIT's family ids (#1367 R5): one progressive tier item
 *  per game, the wallet's shape. */
const uint16_t kOotTier = 106;
const uint16_t kMmTier = 206;

/**
 * THE SHARED-QUANTITY CREDIT WORLD (#1367 R5): the shape that refused every
 * production world under All Locations before the credit. The pair keeps TWO copies
 * of a shared tier family, one per game (the trim's spread), and each game has a
 * location that wants the family's SECOND tier (OoT host 12, MM host 22: the
 * Giant's Wallet shops and bank). On the player's side every kept copy raises the
 * one shared tier, so both are reachable; a proof that credits each copy to its own
 * game only sees one tier on each side. Every host is a location (`allLocations`).
 * Both goals are unconditional, so the locations alone decide the verdict.
 */
void ClBuildCreditWorld() {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotTier);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, 0u);
    ClAddHost(&gClOoT, 12, 0u);
    gClOoT.hostNeedItem[2] = kOotTier; // host 12 wants the second tier
    gClOoT.hostNeedCount[2] = 2;
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;
    gClOoT.withAllLocations = true;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmTier);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, 0u);
    ClAddHost(&gClMM, 22, 0u);
    gClMM.hostNeedItem[2] = kMmTier; // host 22 likewise
    gClMM.hostNeedCount[2] = 2;
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    gClMM.withAllLocations = true;
    ClInstall();
}

/** One credited copy (RSBS_COMBO_BAG_SHARED_CREDIT) of `id`, naming the far game's
 *  id for the family. */
ComboLogicBagItem ClCreditItem(uint8_t origin, uint16_t id, uint16_t farId) {
    ComboLogicBagItem b = ClBagItem(origin, id, RSBS_ITEMCLASS_PROGRESSION);
    b.bagFlags = RSBS_COMBO_BAG_SHARED_CREDIT;
    b.pairId = farId;
    return b;
}

/** Does either coordinator table hold a placement carrying `id`? */
bool ClAnyTableHolds(uint16_t id) {
    const GameId both[2] = { GAME_OOT, GAME_MM };
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < Combo_Logic_PlacementCount(both[s]); ++i) {
            ComboLogicPlacement p;
            if (Combo_Logic_PlacementAt(both[s], i, &p) && p.item.id == id) {
                return true;
            }
        }
    }
    return false;
}

/** The bag both fill worlds use. Five advancement items, both origins. */
int ClBuildFillBag(ComboLogicBagItem* bag) {
    int n = 0;
    bag[n++] = ClBagItem((uint8_t)GAME_OOT, kOotLens, RSBS_ITEMCLASS_PROGRESSION);
    bag[n++] = ClBagItem((uint8_t)GAME_MM, kMmRemains, RSBS_ITEMCLASS_DUNGEON_REWARD);
    bag[n++] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
    bag[n++] = ClBagItem((uint8_t)GAME_MM, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION);
    bag[n++] = ClBagItem((uint8_t)GAME_OOT, kOotHook, RSBS_ITEMCLASS_PROGRESSION);
    return n;
}

int ClRunRound(uint8_t goal, const ComboLogicBagItem* assumed, int assumedCount, ComboLogicRoundResult* out) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.assumed = assumed;
    req.assumedCount = assumedCount;
    req.goal = goal;
    return Combo_Logic_RunRound(&req, out);
}

int ClRunFill(const ComboLogicBagItem* bag, int bagCount, uint8_t goal, uint8_t rung, uint32_t seed,
              ComboLogicFillResult* out) {
    ComboLogicFillRequest req;
    memset(&req, 0, sizeof(req));
    req.bag = bag;
    req.bagCount = bagCount;
    req.goal = goal;
    req.logicRung = rung;
    req.seed = seed;
    req.maxAttempts = 0; // the default batch roll-back count
    return Combo_Logic_RunFill(&req, out);
}

} // namespace

// ============================================================================
// Row 1 — the contract
// ============================================================================

TestResult Test_ComboLogicEngineSurface(void) {
    printf("[TEST] combo-logic-engine-surface: registration, GOAL table, ADR 0002 routing, premise watchdogs\n");

    ClUninstall();

    // --- registration validation -----------------------------------------
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClFillVtable(&gClVtOoT, &gClOoT);

    CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_NONE, &gClVtOoT), "GAME_NONE has no id-space and must be refused");

    {
        ComboLogicEngine bad = gClVtOoT;
        bad.abiVersion = RSBS_COMBO_LOGIC_ENGINE_ABI + 7u;
        CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_OOT, &bad), "an ABI mismatch must be refused, not called through");
        CL_ASSERT(Combo_Logic_GetEngine(GAME_OOT) == NULL, "a refused registration must leave the registry unchanged");
    }
    {
        ComboLogicEngine bad = gClVtOoT;
        bad.expand = NULL;
        CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_OOT, &bad), "a NULL required entry point must be refused");
    }
    {
        // `allEmptyHosts` is REQUIRED, not optional: it is the `none` rung's only
        // host source, and an engine that omitted it would crash inside a fill
        // rather than at registration.
        ComboLogicEngine bad = gClVtOoT;
        bad.allEmptyHosts = NULL;
        CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_OOT, &bad), "a NULL allEmptyHosts must be refused");
    }
    {
        ComboLogicEngine bad = gClVtOoT;
        bad.snapshot = ClSnapshot;
        bad.restore = NULL;
        CL_ASSERT(!Combo_Logic_RegisterEngine(GAME_OOT, &bad),
                  "a snapshot without its restore must be refused: it would silently corrupt the save");
    }

    CL_ASSERT(Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT), "a well-formed engine must register");
    CL_ASSERT(Combo_Logic_GetEngine(GAME_OOT) == &gClVtOoT, "the registry must serve what was registered");
    CL_ASSERT(Combo_Logic_RegisterEngine(GAME_OOT, NULL), "NULL must un-register");
    CL_ASSERT(Combo_Logic_GetEngine(GAME_OOT) == NULL, "un-registration must clear the slot");

    // --- the GOAL truth table ---------------------------------------------
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_BOTH, 1, 1) == 1, "beat-both: 1 AND 1");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_BOTH, 1, 0) == 0, "beat-both: 1 AND 0");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_BOTH, 0, 1) == 0, "beat-both: 0 AND 1");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 1, 0) == 1,
              "beat-either: an unbeatable half is OK");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 0, 1) == 1, "beat-either is symmetric");
    // The OR is never an XOR: both halves provable is a welcome outcome.
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 1, 1) == 1,
              "beat-either must not be narrowed to an XOR (ADR 0010 §1.2)");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_EITHER, 0, 0) == 0, "beat-either: neither half");
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_TRIFORCE_HUNT, 1, 1) == -1,
              "triforce-hunt is a COUNT (answer O10): the boolean form must not answer it "
              "(Combo_Logic_EvaluateTriforceHunt does; locked by combo-triforce-hunt)");
    // OoTMM's single-game goals ('ganon', 'majora'), appended as 4 and 5: ONE
    // half alone, and the other half's answer moves nothing either way.
    for (int o = 0; o <= 1; o++) {
        for (int m = 0; m <= 1; m++) {
            CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_OOT, o, m) == o,
                      "beat-oot must be OoT's half alone, whatever MM's half answers");
            CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_MM, o, m) == m,
                      "beat-mm must be MM's half alone, whatever OoT's half answers");
        }
    }
    CL_ASSERT(Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_OOT, 7, 0) == 1 &&
                  Combo_Logic_EvaluateGoal(RSBS_COMBO_GOAL_BEAT_MM, 0, -3) == 1,
              "a nonzero half answer is TRUE, and the result is exactly 1 (never the raw answer)");
    CL_ASSERT(Combo_Logic_EvaluateGoal(0u, 1, 1) == -1, "an unpinned GOAL value has no evaluator");
    CL_ASSERT(Combo_Logic_EvaluateGoal(6u, 1, 1) == -1,
              "one past the pinned table has no evaluator (a goal a later build appends is refused, not guessed)");

    // ADR 0010 §1.2's creation warning reads the PROOF's halves, never the goal.
    {
        ComboLogicFillResult r;
        memset(&r, 0, sizeof(r));
        CL_ASSERT(Combo_Logic_UnprovedHalves(NULL) == 0u, "no fill result, no warning");
        r.status = RSBS_COMBO_LOGIC_OK;
        r.goalProven = true;
        const struct {
            int o, m;
            uint32_t halves;
        } kHalves[] = { { 1, 1, 0u },
                        { 1, 0, RSBS_COMBO_HALF_MM },
                        { 0, 1, RSBS_COMBO_HALF_OOT },
                        { -1, -1, RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM } };
        for (size_t i = 0; i < sizeof(kHalves) / sizeof(kHalves[0]); i++) {
            r.goalOoT = kHalves[i].o;
            r.goalMM = kHalves[i].m;
            CL_ASSERT(Combo_Logic_UnprovedHalves(&r) == kHalves[i].halves,
                      "a proved world must name exactly the halves its proving round did not prove");
        }
        r.goalOoT = 1;
        r.goalMM = 0;
        r.status = RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE;
        CL_ASSERT(Combo_Logic_UnprovedHalves(&r) == 0u, "a failed fill created no world, so it warns about nothing");
        r.status = RSBS_COMBO_LOGIC_OK;
        r.goalProven = false;
        r.proofSkipped = true;
        CL_ASSERT(Combo_Logic_UnprovedHalves(&r) == (RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM),
                  "rung `none` proved nothing, so both halves carry no proof");
    }

    // --- the fill's refusals, before any world is authored ----------------
    {
        ComboLogicBagItem bag[1];
        ComboLogicFillResult res;
        bag[0] = ClBagItem((uint8_t)GAME_OOT, kOotLens, RSBS_ITEMCLASS_PROGRESSION);

        ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
        ClAddItem(&gClOoT, kOotLens);
        ClAddHost(&gClOoT, 10, 0u);
        ClFillVtable(&gClVtOoT, &gClOoT);
        Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT);
        Combo_Logic_RegisterEngine(GAME_MM, NULL);

        const int ootCallsBefore = gClOoT.placeCalls;
        CL_ASSERT(ClRunFill(bag, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_NO_ENGINE,
                  "a paired fill with one engine must refuse, not half-fill");
        // `res.placed == 0` alone would be satisfied by tables that were empty
        // already. `placeCalls` is a property of THIS call: it moves iff the
        // coordinator asked an engine to place something.
        CL_ASSERT(gClOoT.placeCalls == ootCallsBefore, "and must not have asked the one engine it does have to place");
        CL_ASSERT(res.placed == 0, "a refused fill must place nothing");

        ClResetEngine(&gClMM, (uint8_t)GAME_MM);
        ClAddItem(&gClMM, kMmRemains);
        ClAddHost(&gClMM, 20, 0u);
        ClInstall();

        const int ootCalls2 = gClOoT.placeCalls;
        const int mmCalls2 = gClMM.placeCalls;
        CL_ASSERT(ClRunFill(bag, 1, RSBS_COMBO_GOAL_TRIFORCE_HUNT, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL,
                  "triforce-hunt over engines with no triforcePieces query must refuse rather than be evaluated as "
                  "beat-both");
        CL_ASSERT(gClOoT.placeCalls == ootCalls2 && gClMM.placeCalls == mmCalls2,
                  "an unsupported goal must be caught before anything is placed");
        CL_ASSERT(res.placed == 0 && res.attempts == 0, "and no attempt may have run");
        CL_ASSERT(ClRunFill(bag, 1, RSBS_COMBO_GOAL_BEAT_BOTH, 0u, 1u, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "an unpinned logic rung must refuse");
        CL_ASSERT(ClRunFill(NULL, 3, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "a NULL bag with a nonzero count must refuse");
        {
            ComboLogicBagItem untagged[1];
            untagged[0] = ClBagItem((uint8_t)GAME_NONE, 5u, RSBS_ITEMCLASS_PROGRESSION);
            CL_ASSERT(ClRunFill(untagged, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                          RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                      "an untagged bag entry must refuse (ADR 0002: the origin is the id-space)");
        }
    }

    // --- a pre-attempt refusal describes the CALL, not the tables ---------
    // Every refusal above ran against empty tables, which cannot tell "this call
    // placed nothing" from "the tables happened to be empty". So: fill a world
    // for real, then refuse a call ON TOP of the full tables.
    {
        ComboLogicBagItem pair[2];
        ComboLogicFillResult filled;
        ComboLogicFillResult refused;
        pair[0] = ClBagItem((uint8_t)GAME_OOT, kOotLens, RSBS_ITEMCLASS_PROGRESSION);
        pair[1] = ClBagItem((uint8_t)GAME_MM, kMmRemains, RSBS_ITEMCLASS_DUNGEON_REWARD);

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(pair, 2, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0xA11CEu, &filled) ==
                      RSBS_COMBO_LOGIC_OK,
                  "fill the tables first, so the refusal below has something to be distinguished from");
        CL_ASSERT(filled.placed == 2, "both items placed");

        const int ootPlaced = Combo_Logic_PlacementCount(GAME_OOT);
        const int mmPlaced = Combo_Logic_PlacementCount(GAME_MM);
        const uint32_t tableDigest = Combo_Logic_PlacementDigest();
        const int ootCalls = gClOoT.placeCalls;
        const int mmCalls = gClMM.placeCalls;

        CL_ASSERT(ClRunFill(pair, 2, RSBS_COMBO_GOAL_TRIFORCE_HUNT, RSBS_COMBO_RUNG_BEATABLE, 0xA11CEu, &refused) ==
                      RSBS_COMBO_LOGIC_ERR_UNSUPPORTED_GOAL,
                  "the unsupported goal must still refuse");
        CL_ASSERT(refused.attempts == 0, "no attempt ran");
        CL_ASSERT(refused.placed == 0 && refused.placementDigest == 0u,
                  "a pre-attempt refusal must report on the call — it placed nothing — and must not describe tables it "
                  "never looked at");
        CL_ASSERT(gClOoT.placeCalls == ootCalls && gClMM.placeCalls == mmCalls,
                  "and must not have called either engine");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) == ootPlaced &&
                      Combo_Logic_PlacementCount(GAME_MM) == mmPlaced && Combo_Logic_PlacementDigest() == tableDigest,
                  "and the tables it did not touch must still hold the previous fill, untouched");
    }

    // --- ADR 0002: a foreign item is covered, never stored ----------------
    ClBuildCrossingWorld(0u);
    {
        ComboLogicRoundResult round;
        CL_ASSERT(Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION),
                  "authoring an MM-origin item into an OoT host must succeed");
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round) == RSBS_COMBO_LOGIC_OK, "the round must run");
        // THE COORDINATOR'S HALF. What `place` receives is the coordinator's
        // doing and nothing else's: the origin tag must survive untouched, and
        // the coordinator must not have invented a cover of its own (that is the
        // host engine's obligation, and no assertion here can lock it).
        CL_ASSERT(gClOoT.placedCount == 1, "the OoT engine must have been handed exactly one placement");
        CL_ASSERT(gClOoT.placedRaw[0].originGame == (uint8_t)GAME_MM && gClOoT.placedRaw[0].id == kMmOcarina,
                  "the SharedItem reaching `place` must still carry its MM origin: the coordinator routes by origin "
                  "and never rewrites it");
        CL_ASSERT(gClMM.placeCalls == 0, "and the item's OWN engine must not have been told to host it");
        // THE ENGINE'S HALF, asserted of the stub so the stub's own modelling is
        // visible rather than implied. It is the two engine lanes' obligation,
        // not this PR's: nothing in combo_logic.c can flip these two lines.
        CL_ASSERT(gClOoT.placedIsCover[0], "the stub models the host's table taking a junk COVER");
        CL_ASSERT(gClOoT.placedItem[0].originGame == (uint8_t)GAME_OOT && gClOoT.placedItem[0].id == kOotJunk,
                  "and that cover is a legal LOCAL item");
        CL_ASSERT(ClContractClean(), "no foreign id may reach an engine's assumeOwnItem or its own table");
    }

    // --- premise watchdog 1: a monotone observable that falls -------------
    ClBuildCrossingWorld(0u);
    gClOoT.fault = CL_FAULT_SHRINK;
    ClInstall();
    {
        ComboLogicRoundResult round;
        const int st = ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round);
        CL_ASSERT(st == RSBS_COMBO_LOGIC_ERR_NON_MONOTONE,
                  "a reached set that SHRINKS inside one round must be reported non-monotone");
        CL_ASSERT(round.iterations <= 8, "the violation must be caught where it happens, not looped on");
        CL_ASSERT(gClOoT.endCalls == gClOoT.beginCalls && gClMM.endCalls == gClMM.beginCalls,
                  "a failed round must still close its bracket on every engine it opened");
    }

    // --- premise watchdog 2: `changed` forever ----------------------------
    ClBuildCrossingWorld(0u);
    gClMM.fault = CL_FAULT_ALWAYS_CHANGED;
    ClInstall();
    {
        ComboLogicRoundResult round;
        const int st = ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round);
        CL_ASSERT(st == RSBS_COMBO_LOGIC_ERR_NO_FIXPOINT, "an engine reporting change forever must be bounded");
        CL_ASSERT(round.iterations == RSBS_COMBO_LOGIC_MAX_ROUND_ITERATIONS,
                  "the bound must be the watchdog's, and the loop must stop AT it");
        CL_ASSERT(gClMM.endCalls == gClMM.beginCalls, "the bracket must still close");
    }

    ClUninstall();
    printf("[TEST] combo-logic-engine-surface: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// Row 2 — one round
// ============================================================================

TestResult Test_ComboLogicFixpoint(void) {
    printf("[TEST] combo-logic-fixpoint: termination, order independence, both crossing directions, the pair-level "
           "lock with removal\n");

    // --- both crossing directions in one world, and termination ----------
    ClBuildCrossingWorld(0u);
    {
        ComboLogicRoundResult round;
        CL_ASSERT(Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION),
                  "author the MM Ocarina into OoT's sphere-zero host");
        CL_ASSERT(Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION),
                  "author the OoT Lens into MM's sphere-zero host");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round) == RSBS_COMBO_LOGIC_OK, "the round must run");
        CL_ASSERT(round.iterations >= 2 && round.iterations <= 8,
                  "the fixpoint must terminate in a handful of alternations (audit §4.3 expects two or three)");
        CL_ASSERT(round.exchanged == 2, "exactly two exchanges: one per direction");
        CL_ASSERT(round.goalMM == 1, "OoT -> MM: the Ocarina found in OoT must prove MM's half");
        CL_ASSERT(round.goalOoT == 1, "MM -> OoT: the Lens found in MM must prove OoT's half");
        CL_ASSERT(round.goalExpression == 1, "beat-both holds when both halves do");
        CL_ASSERT(round.candidatesMM >= 1, "MM's host 21, opened by the exchanged Ocarina, must be a candidate");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- order independence ----------------------------------------------
    // The same world, authored in the OPPOSITE order and with the assumed set
    // permuted, must report the same FACTS. The placement digest deliberately is
    // NOT compared: insertion order is world-visible and part of it.
    ComboLogicRoundResult forward;
    ComboLogicRoundResult reverse;
    {
        ComboLogicBagItem assumedA[2];
        ComboLogicBagItem assumedB[2];
        assumedA[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        assumedA[1] = ClBagItem((uint8_t)GAME_MM, kMmBow, RSBS_ITEMCLASS_PROGRESSION);
        assumedB[0] = assumedA[1];
        assumedB[1] = assumedA[0];

        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, assumedA, 2, &forward) == RSBS_COMBO_LOGIC_OK, "round A");

        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, assumedB, 2, &reverse) == RSBS_COMBO_LOGIC_OK, "round B");

        CL_ASSERT(forward.goalOoT == reverse.goalOoT && forward.goalMM == reverse.goalMM &&
                      forward.goalExpression == reverse.goalExpression,
                  "the fixpoint's goal facts must not depend on the order facts arrived in");
        CL_ASSERT(forward.candidatesOoT == reverse.candidatesOoT && forward.candidatesMM == reverse.candidatesMM,
                  "nor must the candidate sets");
        CL_ASSERT(forward.crossingOpenOoT == reverse.crossingOpenOoT &&
                      forward.crossingOpenMM == reverse.crossingOpenMM,
                  "nor the crossing facts");
        CL_ASSERT(forward.exchanged == reverse.exchanged, "nor the number of exchanges");
    }

    // --- the arrival gate -------------------------------------------------
    // OoT's crossing needs the Sword. Until it is held, Termina is unreachable:
    // MM's hosts are not candidates and MM's half does not count as proved, even
    // though MM's own engine would answer yes.
    {
        ComboLogicRoundResult closed;
        ComboLogicRoundResult open;
        ComboLogicBagItem sword[1];

        ClBuildCrossingWorld(1u << 0); // bit 0 == kOotSword
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, NULL, 0, &closed) == RSBS_COMBO_LOGIC_OK, "gated round");
        CL_ASSERT(closed.crossingOpenOoT == 0, "with no Sword, OoT's crossing must be closed");
        CL_ASSERT(closed.candidatesMM == 0, "a Termina the player cannot enter offers no hosts");
        CL_ASSERT(closed.goalMM == 0, "nor may its half be counted as proved");
        CL_ASSERT(closed.exchanged == 0, "nor may an item cross a closed crossing");
        // beat-mm is MM's half ALONE, but that half is the GATED one: Termina is
        // entered through OoT's crossing, so a closed crossing cannot prove it.
        ComboLogicRoundResult closedMm;
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_MM, NULL, 0, &closedMm) == RSBS_COMBO_LOGIC_OK, "gated beat-mm");
        CL_ASSERT(closedMm.crossingOpenOoT == 0 && closedMm.goalExpression == 0,
                  "beat-mm must not prove MM's half through a crossing the player cannot open");

        sword[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, sword, 1, &open) == RSBS_COMBO_LOGIC_OK, "ungated round");
        CL_ASSERT(open.crossingOpenOoT == 1, "the Sword opens OoT's crossing");
        CL_ASSERT(open.candidatesMM >= 1, "and Termina's hosts become candidates");
        CL_ASSERT(open.goalMM == 1, "and MM's half can be proved");
        ComboLogicRoundResult openMm;
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_MM, sword, 1, &openMm) == RSBS_COMBO_LOGIC_OK &&
                      openMm.goalExpression == 1,
                  "and with the crossing open, beat-mm proves");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- THE PAIR-LEVEL LOCK, WITH REMOVAL -------------------------------
    // An OoT-origin progression item hosted ONLY in the MM stub. OoT's goal
    // needs the Lens; no OoT host holds it; the only copy is in Termina.
    {
        ComboLogicRoundResult withHost;
        ComboLogicRoundResult withoutHost;

        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &withHost) == RSBS_COMBO_LOGIC_OK, "paired round");
        CL_ASSERT(withHost.goalExpression == 1,
                  "with the OoT item hosted in MM, the pair's goal must be provable — this is the half that is easy");

        // REMOVAL. Identical world, minus that one host. Without this half the
        // assertion above is theatre: it would pass over any world at all.
        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &withoutHost) == RSBS_COMBO_LOGIC_OK, "removed round");
        CL_ASSERT(withoutHost.goalOoT == 0, "with the only Lens gone, OoT's half must NOT be provable");
        CL_ASSERT(withoutHost.goalExpression == 0, "so beat-both must flip to unprovable");
        // ... and the same world under beat-either is a legitimate world with one
        // unbeatable half (answer O1), not a failure.
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, NULL, 0, &withoutHost) == RSBS_COMBO_LOGIC_OK,
                  "either round");
        CL_ASSERT(withoutHost.goalMM == 1 && withoutHost.goalOoT == 0,
                  "MM's half still proves and OoT's still does not");
        CL_ASSERT(withoutHost.goalExpression == 1, "beat-either permits the unbeatable half");

        // The single-game goals over the same two worlds (beat-both over them is
        // asserted above: provable with the host, flipped without it). With the
        // Lens only in Termina, both halves prove and both goals hold. With it removed, only
        // MM's half proves: beat-mm holds (OoT's unprovable half is no term of
        // it) and beat-oot does not -- the removal flips exactly the goal whose
        // half lost its item.
        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_OOT, NULL, 0, &withHost) == RSBS_COMBO_LOGIC_OK &&
                      withHost.goalExpression == 1,
                  "beat-oot: the Lens hosted in Termina proves OoT's half");
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_MM, NULL, 0, &withHost) == RSBS_COMBO_LOGIC_OK &&
                      withHost.goalExpression == 1,
                  "beat-mm: MM's half proves in the paired world");

        ClBuildCrossingWorld(0u);
        Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_MM, NULL, 0, &withoutHost) == RSBS_COMBO_LOGIC_OK, "beat-mm round");
        CL_ASSERT(withoutHost.goalOoT == 0 && withoutHost.goalMM == 1, "the removal world's halves did not change");
        CL_ASSERT(withoutHost.goalExpression == 1,
                  "beat-mm must hold on MM's half alone: OoT's unprovable half is no term of it");
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_OOT, NULL, 0, &withoutHost) == RSBS_COMBO_LOGIC_OK, "beat-oot round");
        CL_ASSERT(withoutHost.goalExpression == 0,
                  "beat-oot must NOT hold once the only Lens is gone, although MM's half still proves");
    }

    // --- MM's bracket: the re-apply after restore ------------------------
    // A restore that brings the engine's table back EMPTY. The coordinator's
    // re-apply is the only reason the second round still sees the placement —
    // and MM's half depends on harvesting it, so the lock is not vacuous.
    {
        ComboLogicRoundResult first;
        ComboLogicRoundResult second;

        ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
        ClAddItem(&gClOoT, kOotSword);
        ClAddHost(&gClOoT, 10, 0u);
        gClOoT.crossingRequires = 0u;
        gClOoT.goalRequires = 0u;

        ClResetEngine(&gClMM, (uint8_t)GAME_MM);
        ClAddItem(&gClMM, kMmOcarina);
        ClAddItem(&gClMM, kMmBow);
        ClAddItem(&gClMM, kMmRemains);
        ClAddHost(&gClMM, 20, 0u);
        gClMM.crossingRequires = 0u;
        gClMM.goalRequires = ClBit(&gClMM, kMmRemains);
        gClMM.withSnapshot = true;
        gClMM.restoreDropsPlacements = true;
        ClInstall();

        CL_ASSERT(Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_MM, kMmRemains), RSBS_ITEMCLASS_DUNGEON_REWARD),
                  "author MM's own Remains into an MM host");
        CL_ASSERT(gClMM.placedCount == 1, "the engine must hold it");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &first) == RSBS_COMBO_LOGIC_OK, "round 1");
        CL_ASSERT(first.goalMM == 1, "round 1 harvests the Remains from its own reached host");
        CL_ASSERT(gClMM.snapshots == 1 && gClMM.restores == 1, "one snapshot/restore brackets the whole round");
        CL_ASSERT(gClMM.placedCount == 1, "after the restore dropped it, the coordinator must have put it back");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &second) == RSBS_COMBO_LOGIC_OK, "round 2");
        CL_ASSERT(second.goalMM == 1,
                  "round 2 must prove the same half: without the re-apply the table would be empty here");
        CL_ASSERT(gClMM.placedCount == 1, "and the table must still hold it");
        CL_ASSERT(gClOoT.snapshots == 0 && gClOoT.restores == 0,
                  "an engine with pure queries declares no snapshot pair and must not be bracketed");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    ClUninstall();
    printf("[TEST] combo-logic-fixpoint: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// Row 3 — the fill
// ============================================================================

TestResult Test_ComboLogicFill(void) {
    printf("[TEST] combo-logic-fill: determinism with its sensitivity control, the rungs, beat-either's lack of "
           "bias\n");

    ComboLogicBagItem bag[8];
    const int bagCount = ClBuildFillBag(bag);
    ComboLogicFillResult res;

    // --- same seed, same placement; different seed, a different one -------
    uint32_t digestA = 0u;
    uint32_t digestB = 0u;
    uint32_t digestOther = 0u;
    int roundsA = 0;
    {
        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EED1234u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the fill must succeed on a world where both halves prove");
        CL_ASSERT(res.placed == bagCount, "every bag item must be placed");
        CL_ASSERT(res.goalProven && !res.proofSkipped, "the beatable rung must have evaluated the exit condition");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM) == bagCount,
                  "the two tables together must hold the whole bag");
        CL_ASSERT(!res.allLocationsReached && res.locationsChecked == 0,
                  "`beatable` captures no location list and claims no All Locations proof");
        digestA = res.placementDigest;
        roundsA = res.rounds;
        CL_ASSERT(digestA != 0u, "the digest must cover something");

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EED1234u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the same fill must succeed again");
        digestB = res.placementDigest;
        CL_ASSERT(digestA == digestB, "the same seed must place identically: the world is a function of the identity");

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EED1235u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "a neighbouring seed must also succeed");
        digestOther = res.placementDigest;
        // THE SENSITIVITY CONTROL. Without it "same seed, same digest" is
        // satisfied by a fill that ignores the seed entirely.
        CL_ASSERT(digestOther != digestA, "a different seed must place differently");
    }

    // --- WHAT `none` MEANS: all empties, no round ------------------------
    // The world the two host sources disagree about. Its ONLY free host is
    // unreached and never offered as a candidate, so:
    //   `none`     must place there — "bag, then randomly distribute" cannot
    //              dead-end while a check is free (audit §4.3 / ADR 0010 D5);
    //   `beatable` must refuse the SAME world, because the proving rungs ARE
    //              reachability-gated and there is no reached host.
    // Without the second half the first is satisfied by any rung at all; without
    // the first, `none` is a third logic rung wearing the name of no logic.
    //
    // FIRST in this row deliberately: it is the assertion that DISCRIMINATES
    // between the two readings of D5, so it is the one a mutation should trip
    // before any weaker `none` claim below can mask it.
    {
        ComboLogicBagItem one[1];
        ComboLogicPlacement p;
        one[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);

        ClBuildUnreachedOnlyWorld();
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_NONE, 11u, &res) == RSBS_COMBO_LOGIC_OK,
                  "`none` must place into the only free host even though no search reaches it");
        CL_ASSERT(res.placed == 1 && res.attempts == 1 && res.rounds == 0, "one item, one attempt, no round");
        CL_ASSERT(Combo_Logic_GetPlacement(GAME_OOT, 10, &p) && p.item.id == kOotSword,
                  "and the unreached host is where it went");
        CL_ASSERT(gClOoT.beginCalls == 0 && gClMM.beginCalls == 0,
                  "and no query bracket was opened on either engine: the round is not run, not run-and-ignored");
        CL_ASSERT(gClOoT.expands == 0, "nor was anything expanded");

        ClBuildUnreachedOnlyWorld();
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 11u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE,
                  "`beatable` on the identical world must refuse: it draws from REACHED empties and there are none");
        CL_ASSERT(res.placed == 0, "and it placed nothing");
        CL_ASSERT(gClOoT.beginCalls > 0, "having actually run its rounds");
    }

    // --- the `none` rung places without proof; `beatable` refuses --------
    {
        ClBuildFillWorld(false); // MM's goal item is in no bag and no host
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_NONE, 0x0B00u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the base rung is the same fill with the proof skipped: it must succeed");
        CL_ASSERT(res.proofSkipped, "and say so");
        CL_ASSERT(!res.goalProven, "and claim nothing: `not proven` is not `failed`, and it is not `proven` either");
        CL_ASSERT(res.placed == bagCount, "the whole bag must still be distributed");
        CL_ASSERT(res.attempts == 1, "and with no proof obligation there is nothing to retry");
        CL_ASSERT(res.rounds == 0, "and no round may have been run at all: `none` is the absence of the round");

        ClBuildFillWorld(false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x0B00u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "the identical world under `beatable` must refuse: the GOAL is the fill's exit condition");
        CL_ASSERT(!res.goalProven && !res.proofSkipped, "a refused proof is neither proven nor skipped");
        CL_ASSERT(res.attempts == RSBS_COMBO_LOGIC_FILL_RETRIES, "and the batch roll-backs must all have been spent");
    }

    // --- an engine that refuses a host it offered ------------------------
    // The one `place` failure path the fill has. The coordinator's table is the
    // occupancy authority, so a row the engine never accepted would be served by
    // Combo_Logic_GetPlacement, counted, and folded into the digest — the two
    // disagreeing about what a host holds.
    {
        ClBuildFillWorld(true);
        gClOoT.refuseNextNewPlace = true;
        gClMM.refuseNextNewPlace = true; // whichever side the union draw lands on
        ClInstall();

        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0xBADCAFEu, &res) ==
                      RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED,
                  "a refusal on a host the engine itself offered is an engine-contract failure, not a dead end");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) == 0 && Combo_Logic_PlacementCount(GAME_MM) == 0,
                  "and the coordinator must have taken its own row back out: it may not claim a host the engine "
                  "rejected");
        CL_ASSERT(res.placed == 0, "so the result agrees with the tables");
        CL_ASSERT(gClOoT.placedCount == 0 && gClMM.placedCount == 0, "and so do the engines");
        for (uint16_t h = 10; h <= 25; ++h) {
            CL_ASSERT(!Combo_Logic_GetPlacement(GAME_OOT, h, NULL) && !Combo_Logic_GetPlacement(GAME_MM, h, NULL),
                      "no host in either world may read back as occupied");
        }
    }

    // --- beat-either: no bias, and no starved half ------------------------
    {
        // (1) On a world where BOTH halves prove, beat-either and beat-both must
        //     place BYTE-IDENTICALLY for one seed. A fill that ever short-circuited
        //     once one half proved would diverge here, which is the XOR bias ADR
        //     0010 §1.2 forbids.
        uint32_t both = 0u;
        uint32_t either = 0u;

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0xC0FFEEu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-both on the provable world");
        both = res.placementDigest;

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE, 0xC0FFEEu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-either on the same world");
        either = res.placementDigest;
        CL_ASSERT(both == either, "beat-either must not bias the placement toward an asymmetric outcome");

        // (2) A half that cannot prove is PERMITTED, and its checks are not
        //     starved: MM still hosts items.
        ClBuildFillWorld(false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE, 0xC0FFEEu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-either must accept a world whose MM half its own parameters make unbeatable");
        CL_ASSERT(res.goalProven, "the OR held");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_MM) > 0,
                  "the permitted unbeatable half must still receive items, not be emptied");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) > 0, "and so must the provable one");
    }

    // --- the single-game goals: one half's proof, and nothing about the other --
    {
        // (1) No bias: on a world where BOTH halves prove, beat-oot and beat-mm
        //     place byte-identically to beat-both for one seed. The goal is the
        //     loop's EXIT condition, never a steer on the draw.
        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0xB0B0u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-both on the provable world");
        const uint32_t bothDigest = res.placementDigest;
        const uint8_t kSingle[2] = { (uint8_t)RSBS_COMBO_GOAL_BEAT_OOT, (uint8_t)RSBS_COMBO_GOAL_BEAT_MM };
        for (int g = 0; g < 2; g++) {
            ClBuildFillWorld(true);
            CL_ASSERT(ClRunFill(bag, bagCount, kSingle[g], RSBS_COMBO_RUNG_BEATABLE, 0xB0B0u, &res) ==
                          RSBS_COMBO_LOGIC_OK,
                      "a single-game goal on the provable world");
            CL_ASSERT(res.goalProven && res.placementDigest == bothDigest,
                      "a single-game goal must place exactly as beat-both does on a world both halves prove");
            // Both halves proved, so the creation warning names nothing, whatever
            // the goal: the warning reads the proof, not the goal.
            CL_ASSERT(res.goalOoT == 1 && res.goalMM == 1 && Combo_Logic_UnprovedHalves(&res) == 0u,
                      "a world whose halves both prove must report both proved and warn about neither");
        }

        // (2) beat-oot over a world whose MM half is unbeatable by its own
        //     parameters: it PROVES (MM's goal is no term of it), and MM's checks
        //     still receive items. beat-mm and beat-both over the SAME world and
        //     seed do not -- the red half.
        ClBuildFillWorldHalves(true, false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_OOT, RSBS_COMBO_RUNG_BEATABLE, 0xB0B1u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-oot must prove a world whose only unprovable half is MM's");
        CL_ASSERT(res.goalProven, "beat-oot's exit condition held");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_MM) > 0,
                  "beat-oot must still place into MM's checks: the unproved half is filled, not skipped");
        // ADR 0010 §1.2's creation warning: the half that carries no proof is
        // MM's, and it alone is named.
        CL_ASSERT(res.goalOoT == 1 && res.goalMM == 0,
                  "beat-oot's proving round must report OoT's half proved and MM's not");
        CL_ASSERT(Combo_Logic_UnprovedHalves(&res) == RSBS_COMBO_HALF_MM,
                  "beat-oot over an unbeatable MM half must name MM's half, and only it, as carrying no proof");
        // beat-either over the same world proves too, and names the same half.
        ClBuildFillWorldHalves(true, false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE, 0xB0B1u, &res) ==
                          RSBS_COMBO_LOGIC_OK &&
                      Combo_Logic_UnprovedHalves(&res) == RSBS_COMBO_HALF_MM,
                  "beat-either over an unbeatable MM half must prove and name MM's half as carrying no proof");
        ClBuildFillWorldHalves(true, false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_MM, RSBS_COMBO_RUNG_BEATABLE, 0xB0B1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "RED HALF: beat-mm over the same world must be unprovable");
        ClBuildFillWorldHalves(true, false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0xB0B1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "RED HALF: beat-both over the same world must be unprovable");
        CL_ASSERT(res.goalOoT == -1 && res.goalMM == -1 && Combo_Logic_UnprovedHalves(&res) == 0u,
                  "a failed fill reports no proving round and warns about nothing (it created no world)");

        // (3) The mirror: OoT's half unbeatable by its own parameters.
        ClBuildFillWorldHalves(false, true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_MM, RSBS_COMBO_RUNG_BEATABLE, 0xB0B2u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beat-mm must prove a world whose only unprovable half is OoT's");
        CL_ASSERT(res.goalProven && Combo_Logic_PlacementCount(GAME_OOT) > 0,
                  "beat-mm proved and still placed into OoT's checks");
        CL_ASSERT(Combo_Logic_UnprovedHalves(&res) == RSBS_COMBO_HALF_OOT,
                  "beat-mm over an unbeatable OoT half must name OoT's half, and only it, as carrying no proof");
        ClBuildFillWorldHalves(false, true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_OOT, RSBS_COMBO_RUNG_BEATABLE, 0xB0B2u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "RED HALF: beat-oot over the same world must be unprovable");
        ClBuildFillWorldHalves(false, true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0xB0B2u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "RED HALF: beat-both over the same world must be unprovable");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- the all-reachable rung adds a real obligation --------------------
    // An engine that OFFERS a host it does not reach. `beatable` cannot see it
    // (the goal still proves); `all-reachable` must refuse it — and since ABI 8 it
    // refuses AT ONCE, by name, in its pre-flight: the stub has no `allLocations`,
    // so its host table stands in, and host 10 is a location no seed can reach.
    {
        ComboLogicBagItem one[1];
        one[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);

        ClBuildOfferUnreachedWorld();
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 7u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "`beatable` asks only for the GOAL, which this world proves");
        CL_ASSERT(res.goalProven && !res.allHostsReached, "and it reports honestly that a placed host was not reached");

        ClBuildOfferUnreachedWorld();
        const int rc = ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 7u, &res);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE,
                  "`all-reachable` must refuse the same world: its obligation is strictly stronger, and a location "
                  "no seed can reach is refused by the pre-flight");
        CL_ASSERT(res.attempts == 0 && res.rounds == 1 && res.placed == 0,
                  "refused AT ONCE: one pre-flight round, no batch attempt, nothing placed (no ladder of retries "
                  "over a fact of the settings)");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM) == 0,
                  "and the tables the pre-flight emptied stay empty");
        CL_ASSERT(gClOoT.beginCalls == 1 && gClMM.beginCalls == 1, "exactly one round was opened on each engine");
        ComboLogicHostRef ref;
        memset(&ref, 0, sizeof(ref));
        CL_ASSERT(Combo_Logic_UnreachableLocationCount() == 1 && Combo_Logic_UnreachableLocationAt(0, &ref) &&
                      ref.hostGame == (uint8_t)GAME_OOT && ref.hostCheck == 10,
                  "the refusal NAMES the location: OoT host 10, and nothing else");
        CL_ASSERT(!Combo_Logic_UnreachableLocationAt(1, &ref), "and there is exactly one");
        CL_ASSERT(res.locationsChecked == 1 && res.locationsUnreachable == 1,
                  "the result counts the one location captured and the one missed");
        char names[160];
        CL_ASSERT(Combo_Logic_FormatUnreachable(names, sizeof(names), 4) == 1 && names[0] != '\0',
                  "the name line is never empty: a location its game cannot name reads '<game> location <id>'");
        printf("[TEST]   all-reachable refusal names: %s\n", names);
        CL_ASSERT(!Combo_SingleBag_StatusIsWorldDeadEnd(rc, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE),
                  "a pre-flight refusal is never a ladder rung: another seed cannot reach the location either");
        CL_ASSERT(!Combo_SingleBag_StatusIsWorldDeadEnd(RSBS_COMBO_LOGIC_ERR_NOT_ALL_REACHED,
                                                        (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE),
                  "and neither is NOT_ALL_REACHED: after a pre-flight that held, only an engine fault misses one");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");

        // THE NARROW OBLIGATION IS STILL THERE: the same world with host 10 CURATED
        // out of the location list (its game says no seed reaches it) passes the
        // pre-flight, and the offered-but-unreached host it is handed then fails
        // the final round's placed-host term: NOT_ALL_REACHED, over every roll-back.
        ClBuildOfferUnreachedWorld();
        gClOoT.withAllLocations = true;
        gClOoT.notLocation = 1u << 0;
        ClInstall();
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 7u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_NOT_ALL_REACHED,
                  "a placed host that is never reached still fails `all-reachable`");
        CL_ASSERT(res.attempts == RSBS_COMBO_LOGIC_FILL_RETRIES,
                  "and the batch roll-backs must be real roll-backs: every attempt must have re-placed, not "
                  "dead-ended on the previous attempt's leftovers");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- ALL LOCATIONS (ABI 8): every location of BOTH games, whatever the GOAL --
    // The fill world's MM host 24 needs the Bow, which no bag carries: a location no
    // seed can reach. So the pre-flight refuses, naming MM 24, under beat-both AND
    // under beat-oot (OoTMM: every location in both games, whatever the goal). With
    // host 24 curated out, the same world PROVES, with every location reached, and
    // places exactly as `beatable` does (the pre-flight draws no RNG). With OoT's
    // crossing shut, every MM location is unreached (the arrival gate).
    {
        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 0x5EED1234u,
                            &res) == RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE,
                  "All Locations must refuse a world with a location no seed can reach");
        ComboLogicHostRef ref;
        memset(&ref, 0, sizeof(ref));
        CL_ASSERT(Combo_Logic_UnreachableLocationCount() == 1 && Combo_Logic_UnreachableLocationAt(0, &ref) &&
                      ref.hostGame == (uint8_t)GAME_MM && ref.hostCheck == 24,
                  "and name it: MM host 24 (the Bow's), and only it");
        CL_ASSERT(res.locationsChecked == 12 && res.attempts == 0 && res.placed == 0,
                  "twelve locations captured (six per side), refused before any attempt");

        ClBuildFillWorld(true);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_OOT, RSBS_COMBO_RUNG_ALL_REACHABLE, 0x5EED1234u,
                            &res) == RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE,
                  "WHATEVER THE GOAL: under beat-oot a Termina location no seed can reach still refuses");

        ClBuildFillWorld(true);
        gClMM.withAllLocations = true;
        gClMM.notLocation = 1u << 4; // host 24: MM's curated exclusion
        ClInstall();
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 0x5EED1234u,
                            &res) == RSBS_COMBO_LOGIC_OK,
                  "with the unreachable location curated out, All Locations proves the world");
        CL_ASSERT(res.goalProven && res.allLocationsReached && res.allHostsReached,
                  "the GOAL, every placed host AND every location held in the exit round");
        CL_ASSERT(res.locationsChecked == 11 && res.locationsUnreachable == 0 &&
                      Combo_Logic_UnreachableLocationCount() == 0,
                  "eleven locations captured (the curated one left out), none missed");
        CL_ASSERT(res.placementDigest == digestA && res.rounds == roundsA + 1,
                  "the pre-flight draws no RNG and places nothing: the world is `beatable`'s, plus one round");
        CL_ASSERT(!Combo_Logic_GetPlacement(GAME_MM, 24, NULL),
                  "the curated location received no bag row: it is never reached, so never a candidate, and only "
                  "its game's own filler lands there");
        CL_ASSERT(Combo_Logic_UnprovedHalves(&res) == 0u, "and the world names no unproved half");

        ClBuildFillWorld(true);
        gClOoT.crossingRequires = ClBit(&gClOoT, kOotBoots); // in no bag: the crossing never opens
        gClMM.withAllLocations = true;
        gClMM.notLocation = 1u << 4;
        ClInstall();
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_OOT, RSBS_COMBO_RUNG_ALL_REACHABLE, 0x5EED1234u,
                            &res) == RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE,
                  "with OoT's crossing shut, Termina's locations are unreachable, whatever MM's own graph says");
        int mmMissed = 0;
        for (int i = 0; Combo_Logic_UnreachableLocationAt(i, &ref); ++i) {
            mmMissed += (ref.hostGame == (uint8_t)GAME_MM) ? 1 : 0;
        }
        CL_ASSERT(Combo_Logic_UnreachableLocationCount() == 5 && mmMissed == 5,
                  "every one of MM's five locations is named, and no OoT one (the arrival gate)");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- THE GOAL AT THE PRE-FLIGHT (#1367 R5, #1371's deferred refusal) ------
    // The fill world with MM's half unprovable (its goal wants the Mask, in no bag
    // and no host) and MM host 24 curated out, so every location IS reached: the
    // pre-flight's widest reach still misses the GOAL, so no seed proves it. Refused
    // AT ONCE with its own status and the failed half, never the ten batch
    // roll-backs that each end in GOAL_UNPROVABLE (the red half, under `beatable`).
    {
        ClBuildFillWorld(false);
        gClMM.withAllLocations = true;
        gClMM.notLocation = 1u << 4;
        ClInstall();
        const int rc =
            ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 0x5EED1234u, &res);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_ERR_GOAL_UNREACHABLE,
                  "All Locations refuses a goal its pre-flight cannot reach with every required row assumed");
        CL_ASSERT(res.attempts == 0 && res.rounds == 1 && res.placed == 0 && !res.goalProven,
                  "AT ONCE: one pre-flight round, no batch attempt, nothing placed");
        CL_ASSERT(res.goalOoT == 1 && res.goalMM == 0,
                  "and the result names the half that failed (MM's), for the refusal's copy");
        CL_ASSERT(res.locationsUnreachable == 0 && Combo_Logic_UnreachableLocationCount() == 0,
                  "every location was reached: the goal is the refusal, not a location");
        CL_ASSERT(strcmp(Combo_Logic_StatusName(rc), "goal-unreachable") == 0, "its own status name");
        CL_ASSERT(!Combo_SingleBag_StatusIsWorldDeadEnd(rc, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE),
                  "never a ladder rung: another seed cannot reach the goal either");

        ClBuildFillWorld(false);
        gClMM.withAllLocations = true;
        gClMM.notLocation = 1u << 4;
        ClInstall();
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_OOT, RSBS_COMBO_RUNG_ALL_REACHABLE, 0x5EED1234u,
                            &res) == RSBS_COMBO_LOGIC_OK,
                  "the same world under beat-oot (whose goal holds) proves: the refusal is the GOAL's, not MM's half");

        ClBuildFillWorld(false);
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EED1234u, &res) ==
                          RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE &&
                      res.attempts == RSBS_COMBO_LOGIC_FILL_RETRIES,
                  "RED HALF: without the pre-flight (`beatable`) the same goal is found unprovable only after every "
                  "batch roll-back");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- THE SHARED-QUANTITY CREDIT (#1367 R5; combo_logic.h) -----------------
    // ClBuildCreditWorld: one copy of a shared tier family per game, and a location
    // in each game that wants the family's second tier. Credited, every copy counts
    // in both engines (seeded when assumed, exchanged when placed), so All Locations
    // proves; uncredited, each engine sees one tier and the pre-flight refuses both
    // locations (the shape that refused every production world before the credit).
    {
        ComboLogicBagItem plain[2];
        plain[0] = ClBagItem((uint8_t)GAME_OOT, kOotTier, RSBS_ITEMCLASS_PROGRESSION);
        plain[1] = ClBagItem((uint8_t)GAME_MM, kMmTier, RSBS_ITEMCLASS_PROGRESSION);
        ComboLogicBagItem credited[2];
        credited[0] = ClCreditItem((uint8_t)GAME_OOT, kOotTier, kMmTier);
        credited[1] = ClCreditItem((uint8_t)GAME_MM, kMmTier, kOotTier);

        ClBuildCreditWorld();
        CL_ASSERT(ClRunFill(plain, 2, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 0xC4ED17u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE,
                  "RED HALF: uncredited, each engine holds one tier and both second-tier locations are refused");
        ComboLogicHostRef ref;
        memset(&ref, 0, sizeof(ref));
        int named = 0;
        for (int i = 0; Combo_Logic_UnreachableLocationAt(i, &ref); ++i) {
            named += ((ref.hostGame == (uint8_t)GAME_OOT && ref.hostCheck == 12) ||
                      (ref.hostGame == (uint8_t)GAME_MM && ref.hostCheck == 22))
                         ? 1
                         : 0;
        }
        CL_ASSERT(Combo_Logic_UnreachableLocationCount() == 2 && named == 2,
                  "and it names exactly OoT 12 and MM 22, the two second-tier locations");

        ClBuildCreditWorld();
        CL_ASSERT(ClRunFill(credited, 2, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 0xC4ED17u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "credited, the same bag proves All Locations");
        CL_ASSERT(res.goalProven && res.allLocationsReached && res.allHostsReached && res.locationsChecked == 6 &&
                      res.requiredPlaced == 2,
                  "every location of both games reached in the exit round, both copies placed");
        // The exit round assumes NOTHING: the second tier on each side came from the
        // EXCHANGE crediting the other game's placed copy, under the crossing's gate.
        CL_ASSERT(gClOoT.assumeCalls[ClBitIndex(&gClOoT, kOotTier)] > 0 &&
                      gClMM.assumeCalls[ClBitIndex(&gClMM, kMmTier)] > 0,
                  "each engine was credited the other game's copy");
        for (int s = 0; s < 2; ++s) {
            const GameId g = (s == 0) ? GAME_OOT : GAME_MM;
            for (int i = 0; i < Combo_Logic_PlacementCount(g); ++i) {
                ComboLogicPlacement p;
                CL_ASSERT(Combo_Logic_PlacementAt(g, i, &p) && p.pair.originGame == (uint8_t)GAME_NONE,
                          "a credited row is placed as its own item, never as a shared pair");
                const bool ootCopy = p.item.originGame == (uint8_t)GAME_OOT && p.item.id == kOotTier;
                CL_ASSERT((ootCopy && p.credit.originGame == (uint8_t)GAME_MM && p.credit.id == kMmTier) ||
                              (!ootCopy && p.item.originGame == (uint8_t)GAME_MM && p.item.id == kMmTier &&
                               p.credit.originGame == (uint8_t)GAME_OOT && p.credit.id == kOotTier),
                          "the placement records the far game's copy of the family (ComboLogicPlacement.credit)");
                CL_ASSERT(p.hostCheck != 12 && p.hostCheck != 22,
                          "a copy never lands on the location its own family's second tier opens (assumed fill)");
            }
        }
        // AN EXPOSED ROUND AFTER THE FILL credits the placed copies exactly as the
        // fill's own rounds did: nothing assumed, and both second-tier hosts reached.
        {
            ComboLogicRoundResult after;
            CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, nullptr, 0, &after) == RSBS_COMBO_LOGIC_OK &&
                          after.goalExpression == 1 && (gClOoT.reached & (1u << 2)) != 0u &&
                          (gClMM.reached & (1u << 2)) != 0u,
                      "an exposed round over the filled tables reaches OoT 12 and MM 22: the placements carry their "
                      "credit, so a round after the fill answers what the fill's rounds answered");
        }

        // SEEDING, through one exposed round: a credited row assumed in its own game
        // is ALSO assumed in the other's, once.
        ClBuildCreditWorld();
        ComboLogicRoundResult r;
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, credited, 1, &r) == RSBS_COMBO_LOGIC_OK,
                  "one exposed round over one credited OoT copy");
        CL_ASSERT(gClOoT.assumeCalls[ClBitIndex(&gClOoT, kOotTier)] == 1 &&
                      gClMM.assumeCalls[ClBitIndex(&gClMM, kMmTier)] == 1,
                  "the credited copy is assumed once in OoT's engine AND once in MM's, as MM's own id");
        ClBuildCreditWorld();
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, plain, 1, &r) == RSBS_COMBO_LOGIC_OK &&
                      gClMM.assumeCalls[ClBitIndex(&gClMM, kMmTier)] == 0,
                  "and an uncredited copy reaches its own engine only");

        // A shared pair is already credited in both engines: a credit on it too would
        // credit its far half twice, so the request is refused.
        ClBuildCreditWorld();
        ComboLogicBagItem both[1];
        both[0] = credited[0];
        both[0].bagFlags = (uint16_t)(RSBS_COMBO_BAG_SHARED_PAIR | RSBS_COMBO_BAG_SHARED_CREDIT);
        CL_ASSERT(ClRunFill(both, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 0xC4ED17u, &res) ==
                          RSBS_COMBO_LOGIC_ERR_BAD_REQUEST &&
                      res.attempts == 0,
                  "a row that is both a shared pair and a credit is refused before any attempt");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- No Logic: running out of hosts is a CAPACITY refusal, not a rung -----
    // `none` draws from every empty host in one attempt, so ERR_NO_CANDIDATE there
    // says both pools together are smaller than the bag; no seed has more hosts.
    {
        ComboLogicBagItem two[2];
        two[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        two[1] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        ClBuildUnreachedOnlyWorld(); // one free host in both worlds together
        const int rc = ClRunFill(two, 2, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_NONE, 11u, &res);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE && res.attempts == 1 && res.rounds == 0,
                  "two REQUIRED rows over one free host: `none` runs out, in its one attempt, with no round");
        CL_ASSERT(!Combo_SingleBag_StatusIsWorldDeadEnd(rc, (uint8_t)RSBS_COMBO_RUNG_NONE),
                  "under No Logic that is a REFUSAL: the ladder must not re-seed ten times over a capacity fact");
        CL_ASSERT(Combo_SingleBag_StatusIsWorldDeadEnd(rc, (uint8_t)RSBS_COMBO_RUNG_BEATABLE) &&
                      Combo_SingleBag_StatusIsWorldDeadEnd(rc, (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE),
                  "while under a proving rung the same status is the seed's dead end, a rung (the red half)");
        CL_ASSERT(Combo_SingleBag_StatusIsWorldDeadEnd(RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                                                       (uint8_t)RSBS_COMBO_RUNG_BEATABLE),
                  "GOAL_UNPROVABLE stays a rung");
        CL_ASSERT(
            !Combo_SingleBag_StatusIsWorldDeadEnd(RSBS_COMBO_LOGIC_ERR_ABORTED, (uint8_t)RSBS_COMBO_RUNG_BEATABLE),
            "a budget stop never is one");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // --- #1483: the home-capacity words read the counts, never the status -----
    // Combo_SingleBag_HomeShortGames names a game only for a FAILED fill whose
    // counted HOME_ONLY REQUIRED rows outnumber that game's empty hosts.
    {
        ComboSingleBagReport r;
        memset(&r, 0, sizeof(r));
        r.status = RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE;
        CL_ASSERT(Combo_SingleBag_HomeShortGames(&r) == 0u && Combo_SingleBag_HomeShortGames(NULL) == 0u,
                  "a zeroed report (Combo_SingleBag_ClearLastReport's shape) and NULL name no game");
        r.homeRequiredMM = 156;
        r.hostsMM = 155;
        r.homeRequiredOoT = 40;
        r.hostsOoT = 300;
        CL_ASSERT(Combo_SingleBag_HomeShortGames(&r) == RSBS_COMBO_HALF_MM,
                  "one row more than MM's hosts names MM, and only MM (OoT has room)");
        r.homeRequiredMM = 155;
        CL_ASSERT(Combo_SingleBag_HomeShortGames(&r) == 0u, "as many rows as hosts fits: it is not short");
        r.homeRequiredMM = 156;
        r.homeRequiredOoT = 301;
        CL_ASSERT(Combo_SingleBag_HomeShortGames(&r) == (RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM),
                  "both games short name both");
        r.hostsOoT = -1;
        r.hostsMM = -1;
        CL_ASSERT(Combo_SingleBag_HomeShortGames(&r) == 0u,
                  "hosts never counted (-1: the fill did not run an attempt) name no game, whatever the rows");
        r.hostsOoT = 300;
        r.hostsMM = 155;
        r.status = RSBS_COMBO_LOGIC_OK;
        CL_ASSERT(Combo_SingleBag_HomeShortGames(&r) == 0u, "a fill that succeeded is never refused in words");
    }

    // --- the fill's own bracket bookkeeping ------------------------------
    {
        ClBuildFillWorld(true);
        gClMM.withSnapshot = true;
        gClMM.restoreDropsPlacements = true;
        ClInstall();
        CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x1234u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the fill must survive a restore that drops the engine's table every round");
        CL_ASSERT(gClMM.snapshots == res.rounds && gClMM.restores == res.rounds,
                  "exactly one snapshot/restore per round, not per call");
        CL_ASSERT(gClMM.beginCalls == res.rounds && gClMM.endCalls == res.rounds,
                  "and exactly one begin/end per round on each engine");
        CL_ASSERT(gClMM.placedCount == Combo_Logic_PlacementCount(GAME_MM),
                  "at the end the engine's own table must agree with the coordinator's");
        for (int i = 0; i < Combo_Logic_PlacementCount(GAME_MM); ++i) {
            ComboLogicPlacement p;
            CL_ASSERT(Combo_Logic_PlacementAt(GAME_MM, i, &p), "read back the coordinator's MM placement");
            CL_ASSERT(ClOwnPlacementIndex(&gClMM, p.hostCheck) >= 0,
                      "every coordinator placement must be present in the engine after the re-apply");
            ComboLogicPlacement byHost;
            CL_ASSERT(Combo_Logic_GetPlacement(GAME_MM, p.hostCheck, &byHost) && byHost.item.id == p.item.id,
                      "and the occupancy lookup must agree with the ordered table");
            CL_ASSERT(byHost.itemClass != 0u, "the bag row's item class must be carried onto the placement");
        }
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    ClUninstall();
    printf("[TEST] combo-logic-fill: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// Row 4 — the contract's edges (the increment-3 review follow-up, #701)
// ============================================================================
//
// Three properties the first three rows could not see, each one because the
// fixture was too small or too well-behaved to reach it:
//
//   THE HOST-ENUMERATION CAP. Rows 1-3 author at most six hosts per side, so no
//   scratch buffer was ever close to full and it did not matter that the buffer
//   was sized by RSBS_COMBO_LOGIC_PLACEMENT_CAP rather than by a game's check
//   pool. MM's real pool is ~2258 checks and OoT's ~2528, so the first honest
//   engine would have met ERR_CAPACITY on the first bag item of the first fill.
//   Locked with a synthetic pool on both sides of the cap.
//
//   THE FAILED BRACKET. Rows 1-3 never make `beginQuery` or `snapshot` refuse, so
//   two of the three documented triggers of ERR_ENGINE_REFUSED had only a green
//   half. The `began` bookkeeping recorded SUCCESS, so a `beginQuery` that
//   detached and then refused was never handed its `endQuery` and the engine
//   stayed coupled to a simulated save — audit §1.8's hazard, reachable through
//   the contract's own refusal path. Locked in BOTH configurations, because the
//   teardown a part-opened side receives depends on whether it snapshotted:
//   without a snapshot it gets `endQuery` alone (scenario 3), and WITH a
//   successful one it gets `restore`, a full re-apply of its whole table, and then
//   `endQuery` on an engine whose `beginQuery` returned zero (scenario 4). The
//   second is the harder shape and it is the one an engine author will meet first,
//   because MM is the side that snapshots.
//
//   RUNFILL'S RESET. Rows 1-3 author with Combo_Logic_Place OR fill; never both
//   over the same tables. So nothing observed that a fill empties both tables and
//   both engines before its first placement, which is what a spoiler-load caller
//   following Combo_Logic_Place's own invitation would collide with.

TestResult Test_ComboLogicContractEdges(void) {
    printf("[TEST] combo-logic-contract-edges: the host-enumeration cap, the failed bracket's ownership, RunFill's "
           "reset\n");

    ComboLogicFillResult res;
    ComboLogicBagItem one[1];
    one[0] = ClBagItem((uint8_t)GAME_OOT, kOotSword, RSBS_ITEMCLASS_PROGRESSION);

    // --- 1. an offered host list is bounded by the CHECK POOL --------------
    // 2300 hosts: past RSBS_COMBO_LOGIC_PLACEMENT_CAP (2048 since #727 raised it
    // from 1024 to carry the composed bag) and squarely inside the range MM's own
    // ~2258-check pool occupies, so this is the shape of the FIRST fill a real
    // engine drives, not a synthetic extreme. The static assert keeps "past the
    // placement cap, inside the host cap" TRUE rather than a sentence that a later
    // cap raise could silently falsify (#727 caught exactly that: at a 4096
    // placement cap this world would no longer distinguish the two caps).
    {
        const int wide = 2300;
        static_assert(2300 > RSBS_COMBO_LOGIC_PLACEMENT_CAP && 2300 <= RSBS_COMBO_LOGIC_HOST_CAP,
                      "leg 1's world must sit between the placement cap and the host cap");
        const int narrow = 4;

        ClBuildWideHostWorld(wide, narrow);

        // THE TWO SYNTHETIC POOLS ARE DISJOINT, asserted over the world actually
        // built rather than left to the reader of two constants. This is the one
        // world where it could quietly stop being true, and it is what keeps a
        // misrouted host id DETECTABLE here instead of plausible — the ADR 0002 /
        // #356 trap the whole fixture rests on. Both pools are contiguous runs, so
        // their endpoints decide the question.
        {
            const uint16_t ootFirst = ClSyntheticBase(&gClOoT);
            const uint16_t ootLast = (uint16_t)(ootFirst + wide - 1);
            const uint16_t mmFirst = ClSyntheticBase(&gClMM);
            const uint16_t mmLast = (uint16_t)(mmFirst + narrow - 1);

            CL_ASSERT(!ClIsSyntheticHost(&gClMM, ootFirst) && !ClIsSyntheticHost(&gClMM, ootLast) &&
                          !ClIsSyntheticHost(&gClOoT, mmFirst) && !ClIsSyntheticHost(&gClOoT, mmLast),
                      "the two sides' synthetic host pools must be DISJOINT: a shared base would let a host "
                      "misrouted between the engines read as a correct answer in the one world big enough to hide it");
        }

        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_NONE, 0x1D5Fu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "`none` must accept an engine offering far more hosts than the PLACEMENT cap: an enumeration is "
                  "bounded by the game's check pool, and MM's alone is ~2250");
        CL_ASSERT(res.placed == 1, "and the bag item must actually be placed");

        ClBuildWideHostWorld(wide, narrow);
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x1D5Fu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "and so must a PROVING rung, which collects through the very same scratch buffer");
        CL_ASSERT(res.placed == 1 && res.goalProven, "one item placed, the goal proved");
        CL_ASSERT(ClContractClean(), "with no query outside a bracket and no id misrouted between the two pools");
    }

    // --- 2. ... and past the ENUMERATION cap it is refused, not truncated ---
    // The other side of the same line. Without this half, raising the cap could
    // have been "make the buffer big enough that nobody notices truncation".
    //
    // NO RED HALF ON MAIN, stated so the row is not read as stronger than it is:
    // increment 3 refused above ITS cap too (`total > RSBS_COMBO_LOGIC_PLACEMENT_CAP`
    // returned ERR_CAPACITY), so lowering the cap back cannot make this scenario
    // fail — only scenario 1 sees that. What this half locks is a FUTURE
    // coordinator that answers a too-large enumeration by truncating instead of
    // refusing, which no determinism row could see.
    {
        ClBuildWideHostWorld(RSBS_COMBO_LOGIC_HOST_CAP + 1, 4);
        CL_ASSERT(ClRunFill(one, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_NONE, 0x1D5Fu, &res) ==
                      RSBS_COMBO_LOGIC_ERR_CAPACITY,
                  "one host past RSBS_COMBO_LOGIC_HOST_CAP must REFUSE: a truncated candidate list narrows the world "
                  "to a prefix of one engine's table and no determinism row can see it");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) == 0 && Combo_Logic_PlacementCount(GAME_MM) == 0,
                  "and place nothing");
        CL_ASSERT(ClContractClean(), "and the refusal costs no contract violation on the way out");
    }

    // --- 3. a beginQuery that refuses AFTER its detach still gets endQuery --
    // The side that does NOT snapshot: the teardown it is owed is `endQuery` and
    // nothing else. Scenario 4 is the same refusal on a side that DID snapshot.
    {
        ComboLogicRoundResult round;

        ClBuildCrossingWorld(0u);
        gClMM.refuseBeginQuery = true;
        ClInstall();

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round) == RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED,
                  "a refused beginQuery must abort the round as an engine refusal");
        CL_ASSERT(gClMM.beginCalls == 1, "the refusing side was asked to begin");
        CL_ASSERT(gClMM.endCalls == gClMM.beginCalls,
                  "and must have been told to end: endQuery is owed from the CALL, not from the return");
        CL_ASSERT(!gClMM.detached,
                  "so the engine does not stay detached from the live game — the whole reason the rule is per call "
                  "(audit §1.8)");
        CL_ASSERT(gClOoT.beginCalls == 1 && gClOoT.endCalls == 1 && !gClOoT.detached,
                  "and the side that opened BEFORE the refusal is torn down in full");
        CL_ASSERT(gClMM.restores == 0,
                  "and nothing was restored: this side declared no snapshot pair, so the other half of the ownership "
                  "rule is not owed here — which is what makes scenario 4 a DIFFERENT teardown and not a repeat");
        CL_ASSERT(ClContractClean(), "and no query ran outside a bracket");
    }

    // --- 4. ... and a SNAPSHOTTED side that refuses gets restore + re-apply --
    //
    // The configuration the ownership rule was actually written for, and the one no
    // scenario reached before: `snapshot` SUCCEEDS, then `beginQuery` detaches and
    // refuses. The part-opened side is then owed all three halves of the teardown —
    // `restore` (because the blob exists), a full re-apply of its whole placement
    // table through `place`, and `endQuery` on an engine whose `beginQuery` returned
    // zero. MM is the side that snapshots in the real pairing, so this is the shape
    // a real engine meets first, and the header's tolerance paragraph names it.
    //
    // `restoreDropsPlacements` is what makes the re-apply MEASURABLE rather than
    // asserted: the restore hands the engine's table back empty, so the row can only
    // be present afterwards if the coordinator put it back.
    {
        ComboLogicRoundResult round;

        ClBuildCrossingWorld(0u);
        gClMM.withSnapshot = true;
        gClMM.restoreDropsPlacements = true;
        gClMM.refuseBeginQuery = true;
        ClInstall();

        CL_ASSERT(Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION),
                  "author one MM-hosted placement, so the teardown's re-apply has something to put back");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_MM) == 1 && gClMM.placedCount == 1,
                  "both tables hold it before the round");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round) == RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED,
                  "a refused beginQuery aborts the round as an engine refusal, snapshot or no snapshot");
        CL_ASSERT(gClMM.snapshots == 1 && gClMM.beginCalls == 1,
                  "the snapshot was taken and SUCCEEDED, and only then did beginQuery refuse");
        CL_ASSERT(gClMM.restores == 1,
                  "restore is owed from the SNAPSHOT's return, not from the round's: the blob exists and holds the "
                  "pre-round bytes, so leaving it uncalled abandons the live save in query shape");
        CL_ASSERT(gClMM.endCalls == 1,
                  "and endQuery is still owed from the beginQuery CALL, on a side that never opened");
        CL_ASSERT(!gClMM.detached,
                  "so the refusing side is re-coupled to the live game rather than left on a simulated save "
                  "(audit §1.8)");
        CL_ASSERT(gClMM.placedCount == 1 && gClMM.placedHost[0] == 20,
                  "and the FULL re-apply ran on that part-opened side: the harsh restore emptied the engine's table "
                  "and the coordinator put its own row back");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_MM) == 1,
                  "while the coordinator's own table is untouched by the refusal — the refusal describes the round");
        CL_ASSERT(gClOoT.beginCalls == 1 && gClOoT.endCalls == 1 && !gClOoT.detached,
                  "and the side opened before the refusal is torn down in full");
        CL_ASSERT(ClContractClean(), "with no query outside a bracket and no foreign id in MM's own table");
    }

    // --- 5. a refused snapshot is neither queried nor restored -------------
    {
        ComboLogicRoundResult round;

        ClBuildCrossingWorld(0u);
        gClMM.withSnapshot = true;
        gClMM.refuseSnapshot = true;
        ClInstall();

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &round) == RSBS_COMBO_LOGIC_ERR_ENGINE_REFUSED,
                  "a refused snapshot must abort the round as an engine refusal");
        CL_ASSERT(gClMM.snapshots == 1, "the snapshot was attempted");
        CL_ASSERT(gClMM.beginCalls == 0,
                  "and nothing was begun on that side: a failed snapshot must never be followed by a query");
        CL_ASSERT(gClMM.endCalls == 0,
                  "so nothing is owed back either — endQuery follows beginQuery, and beginQuery never ran");
        CL_ASSERT(gClMM.restores == 0, "and nothing may be restored out of a blob that was never captured");
        CL_ASSERT(gClOoT.beginCalls == 1 && gClOoT.endCalls == 1 && !gClOoT.detached,
                  "while the side opened before it is still torn down in full");
        CL_ASSERT(ClContractClean(), "and no query ran on a side whose snapshot refused");
    }

    // --- 6. RunFill takes the tables EMPTY, on both fill paths --------------
    // Authored rows carrying ids that are in NO bag (Boots, Mask), so "did the
    // fill keep them" is answerable without depending on where the union draw
    // sent the bag's own items.
    {
        ComboLogicBagItem bag[8];
        const int bagCount = ClBuildFillBag(bag);
        const uint8_t rungs[2] = { RSBS_COMBO_RUNG_NONE, RSBS_COMBO_RUNG_BEATABLE };

        for (int r = 0; r < 2; ++r) {
            ClBuildFillWorld(true);
            CL_ASSERT(Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_OOT, kOotBoots), RSBS_ITEMCLASS_PROGRESSION),
                      "author one row the way a spoiler load would");
            CL_ASSERT(Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_MM, kMmMask), RSBS_ITEMCLASS_PROGRESSION),
                      "and one on the other side");
            CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) == 1 && Combo_Logic_PlacementCount(GAME_MM) == 1,
                      "both tables hold their authored row");
            CL_ASSERT(ClAnyTableHolds(kOotBoots) && ClAnyTableHolds(kMmMask), "by id, before the fill");

            CL_ASSERT(ClRunFill(bag, bagCount, RSBS_COMBO_GOAL_BEAT_BOTH, rungs[r], 0x0FFu, &res) ==
                          RSBS_COMBO_LOGIC_OK,
                      "the fill over those tables must succeed");
            CL_ASSERT(res.placed == bagCount,
                      "and hold ONLY its own bag: every attempt begins with Combo_Logic_ResetPlacements(), so an "
                      "authored partial world is discarded, not extended");
            CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM) == bagCount,
                      "the two tables agree with the result");
            CL_ASSERT(!ClAnyTableHolds(kOotBoots) && !ClAnyTableHolds(kMmMask),
                      "and neither authored item survives anywhere in either table");
            CL_ASSERT(gClOoT.placedCount + gClMM.placedCount == bagCount,
                      "and the ENGINES forgot too: the reset calls clearPlacements on both sides");
            CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
        }
    }

    ClUninstall();
    printf("[TEST] combo-logic-contract-edges: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// Row 4b — the round's forbidden hosts (ABI 6, epic #575 H4)
// ============================================================================
//
// THE WORLD. Each side hosts one item of its OWN origin and one of the OTHER's,
// so every way a host's contents can reach a round is present once:
//   OoT host 10 (sphere 0)        holds MM's Ocarina  -> exchanged to MM
//   OoT host 11 (needs Sword)     holds OoT's Lens    -> harvested by OoT, OoT's goal
//   OoT host 12 (sphere 0)        holds nothing       -> the empty-host control
//   MM  host 20 (sphere 0)        holds OoT's Sword   -> exchanged to OoT, opens 11
//   MM  host 21 (needs Ocarina)   holds MM's Remains  -> harvested by MM, MM's goal
// MM snapshots, so the bracket's restore and re-apply run in every round.
// beat-both holds with nothing forbidden, through both exchanges and both
// harvests.

namespace {

void ClBuildForbiddenWorld() {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, ClBit(&gClOoT, kOotSword));
    ClAddHost(&gClOoT, 12, 0u);
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = ClBit(&gClOoT, kOotLens);

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, ClBit(&gClMM, kMmOcarina));
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = ClBit(&gClMM, kMmRemains);
    gClMM.withSnapshot = true;
    ClInstall();
}

bool ClPlaceForbiddenWorld() {
    return Combo_Logic_Place(GAME_OOT, 10, ClItem((uint8_t)GAME_MM, kMmOcarina), RSBS_ITEMCLASS_PROGRESSION) &&
           Combo_Logic_Place(GAME_OOT, 11, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION) &&
           Combo_Logic_Place(GAME_MM, 20, ClItem((uint8_t)GAME_OOT, kOotSword), RSBS_ITEMCLASS_PROGRESSION) &&
           Combo_Logic_Place(GAME_MM, 21, ClItem((uint8_t)GAME_MM, kMmRemains), RSBS_ITEMCLASS_PROGRESSION);
}

ComboLogicHostRef ClHostRef(uint8_t game, uint16_t check) {
    ComboLogicHostRef h;
    memset(&h, 0, sizeof(h));
    h.hostGame = game;
    h.hostCheck = check;
    return h;
}

/** One beat-both round over the current tables, nothing assumed, `count` hosts
 *  forbidden. `forbidden` is passed through verbatim (NULL included), so the
 *  malformed shapes reach the coordinator as written. */
int ClRunForbidden(const ComboLogicHostRef* forbidden, int count, ComboLogicRoundResult* out) {
    ComboLogicRoundRequest req;
    memset(&req, 0, sizeof(req));
    req.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH;
    req.forbidden = forbidden;
    req.forbiddenCount = count;
    memset(out, 0, sizeof(*out));
    return Combo_Logic_RunRound(&req, out);
}

void ClPrintForbiddenRound(const char* tag, int rc, const ComboLogicRoundResult& r) {
    // OoT's stub keeps its round state after `endQuery` (no snapshot pair), so its
    // Lens count is the round's own; MM's is restored by its snapshot and is not
    // printed. MM's Ocarina assumes are cumulative across rounds.
    printf("[TEST]   %s: rc=%s goalOoT=%d goalMM=%d expr=%d exchanged=%d allHostsReached=%d | OoT Lens copies %d, MM "
           "Ocarina assumes %d (cumulative) | withhold calls OoT %d MM %d\n",
           tag, Combo_Logic_StatusName(rc), r.goalOoT, r.goalMM, r.goalExpression, r.exchanged, r.allHostsReached,
           gClOoT.haveCount[ClBitIndex(&gClOoT, kOotLens)], gClMM.assumeCalls[ClBitIndex(&gClMM, kMmOcarina)],
           gClOoT.withholdCalls, gClMM.withholdCalls);
}

} // namespace

TestResult Test_ComboLogicForbiddenHost(void) {
    printf("[TEST] combo-logic-forbidden-host: a forbidden host's item is never harvested or exchanged, and an empty "
           "forbidden set changes no round (ABI 6, #575 H4)\n");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;

    ClBuildForbiddenWorld();
    CL_ASSERT(ClPlaceForbiddenWorld(), "the four authored placements were accepted");

    // --- 0. the baseline: nothing forbidden ---------------------------------
    ComboLogicRoundResult base;
    const int baseRc = ClRunForbidden(nullptr, 0, &base);
    ClPrintForbiddenRound("baseline", baseRc, base);
    CL_ASSERT(baseRc == RSBS_COMBO_LOGIC_OK && base.goalOoT == 1 && base.goalMM == 1 && base.goalExpression == 1,
              "0: the world proves beat-both with nothing forbidden");
    CL_ASSERT(base.exchanged == 2 && base.allHostsReached == 1,
              "0: both crossings are exchanged and every placed host is reached");

    // --- 1. an OWN-origin host: withheld by its engine ----------------------
    {
        const ComboLogicHostRef f[1] = { ClHostRef(O, 11) };
        const int lensBefore = gClOoT.assumeCalls[ClBitIndex(&gClOoT, kOotLens)];
        const int ootCallsBefore = gClOoT.withholdCalls;
        const int mmCallsBefore = gClMM.withholdCalls;
        ComboLogicRoundResult r;
        const int rc = ClRunForbidden(f, 1, &r);
        ClPrintForbiddenRound("forbid OoT 11 (own: OoT's Lens)", rc, r);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_OK, "1: the round runs");
        CL_ASSERT(r.goalOoT == 0 && r.goalExpression == 0,
                  "1: OoT's Lens on a forbidden OoT host was HARVESTED anyway — OoT's goal still reads proved");
        CL_ASSERT(gClOoT.haveCount[ClBitIndex(&gClOoT, kOotLens)] == 0 &&
                      gClOoT.assumeCalls[ClBitIndex(&gClOoT, kOotLens)] == lensBefore,
                  "1: the round ended holding a Lens copy");
        CL_ASSERT(r.goalMM == 1 && r.exchanged == 2, "1: the other half and both crossings are untouched");
        CL_ASSERT(r.allHostsReached == 1, "1: a withheld host is still a REACHED host; only its contents vanish");
        CL_ASSERT(gClOoT.withholdCalls == ootCallsBefore + 1 && gClMM.withholdCalls == mmCallsBefore,
                  "1: the withhold went to the host's own engine, once, and to no other");
    }

    // --- 2. a CROSS-origin host on OoT: withheld by the coordinator ---------
    {
        const ComboLogicHostRef f[1] = { ClHostRef(O, 10) };
        const int ocarinaBefore = gClMM.assumeCalls[ClBitIndex(&gClMM, kMmOcarina)];
        ComboLogicRoundResult r;
        const int rc = ClRunForbidden(f, 1, &r);
        ClPrintForbiddenRound("forbid OoT 10 (cross: MM's Ocarina)", rc, r);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_OK, "2: the round runs");
        CL_ASSERT(gClMM.assumeCalls[ClBitIndex(&gClMM, kMmOcarina)] == ocarinaBefore && r.exchanged == 1,
                  "2: MM's Ocarina on a forbidden OoT host was EXCHANGED anyway");
        CL_ASSERT(r.goalMM == 0 && r.goalOoT == 1 && r.goalExpression == 0,
                  "2: MM's half (Remains behind the Ocarina) falls and OoT's half stands");
    }

    // --- 3. a CROSS-origin host on MM: withheld by the coordinator ----------
    {
        const ComboLogicHostRef f[1] = { ClHostRef(M, 20) };
        ComboLogicRoundResult r;
        const int rc = ClRunForbidden(f, 1, &r);
        ClPrintForbiddenRound("forbid MM 20 (cross: OoT's Sword)", rc, r);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_OK && r.exchanged == 1,
                  "3: OoT's Sword on a forbidden MM host was EXCHANGED anyway");
        CL_ASSERT(r.goalOoT == 0 && r.goalMM == 1, "3: OoT's half (Lens behind the Sword) falls, MM's stands");
        CL_ASSERT(r.allHostsReached == 0, "3: OoT host 11 is unreached without the Sword, and the round says so");
    }

    // --- 4. an OWN-origin host on MM, across MM's snapshot bracket ----------
    {
        const ComboLogicHostRef f[1] = { ClHostRef(M, 21) };
        ComboLogicRoundResult r;
        const int rc = ClRunForbidden(f, 1, &r);
        ClPrintForbiddenRound("forbid MM 21 (own: MM's Remains)", rc, r);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_OK && r.goalMM == 0 && r.goalOoT == 1,
                  "4: MM's Remains on a forbidden MM host was HARVESTED anyway");
        CL_ASSERT(r.exchanged == 2 && r.allHostsReached == 1,
                  "4: both crossings still exchange and the withheld host is still reached");
    }

    // --- 5. an EMPTY forbidden set, and a forbidden EMPTY host, change nothing
    {
        const ComboLogicHostRef unused[1] = { ClHostRef(O, 11) };
        ComboLogicRoundResult emptyArray;
        const int rcArray = ClRunForbidden(unused, 0, &emptyArray);
        CL_ASSERT(rcArray == RSBS_COMBO_LOGIC_OK && memcmp(&emptyArray, &base, sizeof(base)) == 0,
                  "5: a non-NULL forbidden array with a zero count changed the round");

        const ComboLogicHostRef emptyHost[1] = { ClHostRef(O, 12) };
        ComboLogicRoundResult r;
        const int rc = ClRunForbidden(emptyHost, 1, &r);
        ClPrintForbiddenRound("forbid OoT 12 (holds nothing)", rc, r);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_OK && memcmp(&r, &base, sizeof(base)) == 0,
                  "5: forbidding a host that holds nothing changed the round");
    }

    // --- 6. nothing leaks into the next round --------------------------------
    {
        const ComboLogicHostRef f[2] = { ClHostRef(O, 11), ClHostRef(M, 20) };
        ComboLogicRoundResult both;
        const int rcBoth = ClRunForbidden(f, 2, &both);
        ClPrintForbiddenRound("forbid OoT 11 + MM 20", rcBoth, both);
        CL_ASSERT(rcBoth == RSBS_COMBO_LOGIC_OK && both.goalOoT == 0 && both.exchanged == 1,
                  "6: two forbidden hosts in one round");
        ComboLogicRoundResult after;
        const int rcAfter = ClRunForbidden(nullptr, 0, &after);
        CL_ASSERT(rcAfter == RSBS_COMBO_LOGIC_OK && memcmp(&after, &base, sizeof(base)) == 0,
                  "6: the round AFTER a forbidden round differs from the baseline — the withhold leaked");
        CL_ASSERT(gClOoT.withheld == 0u && gClMM.withheld == 0u, "6: an engine kept a withheld host past endQuery");
    }

    // --- 7. refusals, all before any bracket opens ----------------------------
    {
        const int beginsBefore = gClOoT.beginCalls + gClMM.beginCalls;
        ComboLogicRoundResult r;
        CL_ASSERT(ClRunForbidden(nullptr, -1, &r) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "7: a negative forbidden count is refused");
        CL_ASSERT(ClRunForbidden(nullptr, 1, &r) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "7: a NULL forbidden array with a positive count is refused");
        const ComboLogicHostRef none[1] = { ClHostRef((uint8_t)GAME_NONE, 10) };
        CL_ASSERT(ClRunForbidden(none, 1, &r) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "7: a forbidden host of no game is refused");

        // An engine that cannot withhold: a round forbidding one of ITS hosts is
        // refused; a round forbidding only the OTHER engine's hosts still runs.
        gClVtOoT.withholdHost = nullptr;
        CL_ASSERT(Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT), "7: an ABI-6 engine without withholdHost registers");
        const ComboLogicHostRef ootHost[1] = { ClHostRef(O, 11) };
        CL_ASSERT(ClRunForbidden(ootHost, 1, &r) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "7: a forbidden host of an engine with no withholdHost is refused, not ignored");
        CL_ASSERT(gClOoT.beginCalls + gClMM.beginCalls == beginsBefore, "7: every refusal came before any beginQuery");
        const ComboLogicHostRef mmHost[1] = { ClHostRef(M, 21) };
        CL_ASSERT(ClRunForbidden(mmHost, 1, &r) == RSBS_COMBO_LOGIC_OK && r.goalMM == 0 && r.goalOoT == 1,
                  "7: forbidding only the other engine's host still runs");
        gClVtOoT.withholdHost = ClWithholdHost;
        CL_ASSERT(Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT), "7: the withholding engine re-registers");
    }

    CL_ASSERT(gClOoT.withholdUnknown == 0 && gClMM.withholdUnknown == 0,
              "a withhold named a host its engine does not own (misrouted across games)");
    CL_ASSERT(ClContractClean(), "the contract traps must stay clear");

    ClUninstall();
    printf("[TEST] combo-logic-forbidden-host: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// Row 5 — the multiplicity contract and the bag model (ABI 3, 2026-09-26)
// ============================================================================
//
// THE OPERATOR'S RULING, over stub engines, in two kinds of leg — and the
// difference matters, so it is stated rather than left to the reader:
//
//   - M1-M3 are CONTRACT-SHAPE REGRESSION GUARDS OVER THE STUB. The ABI-2 de-dup
//     lived in the two REAL engines, not in combo_logic.c, so these legs would
//     also pass against the pre-ABI-3 coordinator plus this counting stub. What
//     they guard is the coordinator's side of the shape: one call per row, one
//     exchange per placement per round, and the fill carrying every copy to the
//     engines. The ENGINE fix (counting, clamping, order independence) is locked
//     over the real engines by the rando-tier row `combo-logic-multiplicity`.
//   - S-a..S-f lock THE BAG MODEL's surplus / filler / exact-fit rules, which are
//     coordinator code and new in ABI 3, so these legs can and do go red against
//     a wrong coordinator.

namespace {

/** Every host open, both goals free: a world about COUNTS and SHAPES only. */
void ClBuildOpenWorld(int ootHosts, int mmHosts) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddItem(&gClOoT, kOotBoots);
    for (int i = 0; i < ootHosts; ++i) {
        ClAddHost(&gClOoT, (uint16_t)(10 + i), 0u);
    }
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddItem(&gClMM, kMmMask);
    for (int i = 0; i < mmHosts; ++i) {
        ClAddHost(&gClMM, (uint16_t)(40 + i), 0u);
    }
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    ClInstall();
}

int ClCallsFor(const ClEngine* e, uint16_t id) {
    const int k = ClBitIndex(e, id);
    return (k >= 0) ? e->assumeCalls[k] : -1;
}

/** Does every coordinator placement carry a bag row, with no row used more often
 *  than the bag holds it? The "the coordinator places bag rows and NOTHING ELSE"
 *  half of shape 4 — no filler, no trap, no invented copy. */
bool ClPlacementsAreBagRows(const ComboLogicBagItem* bag, int bagCount) {
    int used[RSBS_COMBO_LOGIC_BAG_CAP];
    memset(used, 0, sizeof(used));
    const GameId both[2] = { GAME_OOT, GAME_MM };
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < Combo_Logic_PlacementCount(both[s]); ++i) {
            ComboLogicPlacement p;
            Combo_Logic_PlacementAt(both[s], i, &p);
            bool matched = false;
            for (int b = 0; b < bagCount && !matched; ++b) {
                if (!used[b] && bag[b].item.originGame == p.item.originGame && bag[b].item.id == p.item.id) {
                    used[b] = 1;
                    matched = true;
                }
            }
            if (!matched) {
                return false;
            }
        }
    }
    return true;
}

/** Leftover hosts of one side: counted, ascending, and disjoint from the table. */
bool ClLeftoverConsistent(GameId g, int expected) {
    uint16_t hosts[kClMaxHosts];
    const int n = Combo_Logic_LeftoverHosts(g, hosts, kClMaxHosts);
    if (n != expected) {
        printf("[TEST]   %s leftover=%d expected=%d\n", Game_ToString(g), n, expected);
        return false;
    }
    for (int i = 0; i < n; ++i) {
        if (i > 0 && hosts[i] <= hosts[i - 1]) {
            return false;
        }
        if (Combo_Logic_GetPlacement(g, hosts[i], NULL)) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace {

// ---------------------------------------------------------------------------
// Leg C's synthetic classification sources. OoT: 1 progression, 2 progression
// confined by keysanity, 3 renewable, 4 junk, 5 trap, 6 progression feeding the
// bombchu kind. MM: 1 progression, 2 RENEWABLE feeding the bombchu kind (so it
// reconciles to OoT id 6's PROGRESSION), 3 trap, 4 junk, 5 progression that needs
// the souls give capability. Every other id is not a fill item.
// ---------------------------------------------------------------------------
int ClComposeOoTClassify(uint16_t id, ComboItemClassRow* out) {
    ComboItemClassRow row = { RSBS_FILL_CLASS_NONE, 0u, 0u, 0u };
    switch (id) {
        case 1:
            row.fillClass = RSBS_FILL_CLASS_PROGRESSION;
            break;
        case 2:
            row.fillClass = RSBS_FILL_CLASS_PROGRESSION;
            row.armedBy = RSBS_FILL_ARM_SMALL_KEYS_ROAM;
            break;
        case 3:
            row.fillClass = RSBS_FILL_CLASS_RENEWABLE;
            break;
        case 4:
            row.fillClass = RSBS_FILL_CLASS_JUNK;
            break;
        case 5:
            row.fillClass = RSBS_FILL_CLASS_TRAP;
            break;
        case 6:
            row.fillClass = RSBS_FILL_CLASS_PROGRESSION;
            row.sharedKind = RSBS_SHARED_RES_BOMBCHU_COUNT;
            break;
        default:
            *out = row;
            return 0;
    }
    *out = row;
    return 1;
}

int ClComposeMMClassify(uint16_t id, ComboItemClassRow* out) {
    ComboItemClassRow row = { RSBS_FILL_CLASS_NONE, 0u, 0u, 0u };
    switch (id) {
        case 1:
            row.fillClass = RSBS_FILL_CLASS_PROGRESSION;
            break;
        case 2:
            row.fillClass = RSBS_FILL_CLASS_RENEWABLE;
            row.sharedKind = RSBS_SHARED_RES_BOMBCHU_COUNT;
            break;
        case 3:
            row.fillClass = RSBS_FILL_CLASS_TRAP;
            break;
        case 4:
            row.fillClass = RSBS_FILL_CLASS_JUNK;
            break;
        case 5:
            row.fillClass = RSBS_FILL_CLASS_PROGRESSION;
            row.armedBy = RSBS_FILL_ARM_SOULS;
            break;
        default:
            *out = row;
            return 0;
    }
    *out = row;
    return 1;
}

const ComboItemClassSource kClComposeOoTSource = { RSBS_ITEM_CLASS_SOURCE_ABI, 16u, ClComposeOoTClassify };
const ComboItemClassSource kClComposeMMSource = { RSBS_ITEM_CLASS_SOURCE_ABI, 16u, ClComposeMMClassify };

ComboLogicPoolRow ClPoolRow(uint8_t origin, uint16_t id, uint16_t poolFlags) {
    ComboLogicPoolRow row;
    memset(&row, 0, sizeof(row));
    row.item.originGame = origin;
    row.item.id = id;
    row.poolFlags = poolFlags;
    return row;
}

/** Swaps both real classification sources for the synthetic pair and puts the
 *  real ones back on EVERY exit of leg C (an early CL_ASSERT return included),
 *  so a failure here cannot leave a later row classifying through a fixture. */
struct ClComposeSourceGuard {
    const ComboItemClassSource* realOoT;
    const ComboItemClassSource* realMM;
    bool installed = false;
    ClComposeSourceGuard()
        : realOoT(Combo_GetItemClassSource((uint8_t)GAME_OOT)), realMM(Combo_GetItemClassSource((uint8_t)GAME_MM)) {
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_OOT);
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_MM);
        installed = Combo_RegisterItemClassSource((uint8_t)GAME_OOT, &kClComposeOoTSource) &&
                    Combo_RegisterItemClassSource((uint8_t)GAME_MM, &kClComposeMMSource);
    }
    ~ClComposeSourceGuard() {
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_OOT);
        Combo_TestUnregisterItemClassSource((uint8_t)GAME_MM);
        if (realOoT != nullptr) {
            Combo_RegisterItemClassSource((uint8_t)GAME_OOT, realOoT);
        }
        if (realMM != nullptr) {
            Combo_RegisterItemClassSource((uint8_t)GAME_MM, realMM);
        }
    }
};

TestResult ClComposeLeg(void) {
    const ClComposeSourceGuard guard;
    CL_ASSERT(guard.installed, "C: both synthetic classification sources installed");

    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    const uint16_t P = RSBS_COMBO_POOL_PLENTIFUL;
    const ComboLogicPoolRow pool[] = {
        ClPoolRow(O, 1, 0), ClPoolRow(O, 1, P), ClPoolRow(O, 2, 0), ClPoolRow(O, 3, 0), ClPoolRow(O, 4, P),
        ClPoolRow(O, 5, 0), ClPoolRow(M, 1, 0), ClPoolRow(M, 2, 0), ClPoolRow(M, 2, P), ClPoolRow(M, 3, 0),
        ClPoolRow(M, 4, 0), ClPoolRow(M, 5, 0), ClPoolRow(O, 1, 0), ClPoolRow(O, 6, 0),
    };
    const int poolCount = (int)(sizeof(pool) / sizeof(pool[0]));
    ComboLogicComposeRequest req;
    memset(&req, 0, sizeof(req));
    req.rows = pool;
    req.rowCount = poolCount;
    // THE SHARED-QUANTITY TRIM (lane K13) refuses an unpublished starting bar; this
    // pool holds no trim-family row, so any published value leaves the rule alone.
    req.startingHealthOoT = 0x30u;
    req.startingHealthMM = 0x30u;

    // C1. THE RULE TABLE, nothing armed.
    ComboLogicBagItem bag[16];
    int index[16];
    ComboLogicComposeResult res;
    CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_OK, "C1: the compose succeeds");
    // pool order, REQUIRED and SURPLUS only: O1, O1(surplus), M1, M2 (renewable,
    // reconciled to PROGRESSION through the bombchu kind), M2(surplus), O1, O6.
    const int expectIndex[] = { 0, 1, 6, 7, 8, 12, 13 };
    const uint16_t expectFlags[] = { 0, RSBS_COMBO_BAG_SURPLUS, 0, 0, RSBS_COMBO_BAG_SURPLUS, 0, 0 };
    CL_ASSERT(res.bagCount == 7, "C1: exactly the progression copies of both pools enter the bag");
    for (int i = 0; i < 7; ++i) {
        CL_ASSERT(index[i] == expectIndex[i], "C1: bag rows keep POOL ORDER (a stable filter)");
        CL_ASSERT(bag[i].item.originGame == pool[index[i]].item.originGame && bag[i].item.id == pool[index[i]].item.id,
                  "C1: each bag row is its pool row's copy");
        CL_ASSERT(bag[i].bagFlags == expectFlags[i], "C1: a PLENTIFUL progression copy is SURPLUS, any other REQUIRED");
        CL_ASSERT(bag[i].itemClass == 0u, "C1: the ADR 0011 selection bit is the draw's, not the builder's");
        CL_ASSERT(Combo_ItemClassOf(bag[i].item) == RSBS_FILL_CLASS_PROGRESSION,
                  "C1: NO trap, junk or renewable row is ever in the bag");
    }
    const ComboLogicComposeCounts& co = res.perGame[O];
    const ComboLogicComposeCounts& cm = res.perGame[M];
    CL_ASSERT(co.rows[RSBS_COMBO_COMPOSE_REQUIRED] == 3 && co.rows[RSBS_COMBO_COMPOSE_SURPLUS] == 1 &&
                  co.rows[RSBS_COMBO_COMPOSE_CONFINED] == 1 && co.rows[RSBS_COMBO_COMPOSE_RENEWABLE] == 1 &&
                  co.rows[RSBS_COMBO_COMPOSE_JUNK] == 1 && co.rows[RSBS_COMBO_COMPOSE_TRAP] == 1 &&
                  co.rows[RSBS_COMBO_COMPOSE_UNCLASSIFIED] == 0 && co.plentiful == 2,
              "C1: OoT's rows are counted per disposition (the PLENTIFUL junk copy is junk, not surplus)");
    CL_ASSERT(cm.rows[RSBS_COMBO_COMPOSE_REQUIRED] == 2 && cm.rows[RSBS_COMBO_COMPOSE_SURPLUS] == 1 &&
                  cm.rows[RSBS_COMBO_COMPOSE_CONFINED] == 1 && cm.rows[RSBS_COMBO_COMPOSE_RENEWABLE] == 0 &&
                  cm.rows[RSBS_COMBO_COMPOSE_JUNK] == 1 && cm.rows[RSBS_COMBO_COMPOSE_TRAP] == 1 && cm.plentiful == 1,
              "C1: MM's rows are counted per disposition; the renewable bombchu row reconciled to progression");
    CL_ASSERT(Combo_Logic_ComposeDisposition(pool[2].item, 0u, 0u) == RSBS_COMBO_COMPOSE_CONFINED &&
                  Combo_Logic_ComposeDisposition(pool[2].item, 0u, RSBS_FILL_ARM_SMALL_KEYS_ROAM) ==
                      RSBS_COMBO_COMPOSE_REQUIRED &&
                  Combo_Logic_ComposeDisposition(pool[2].item, P, RSBS_FILL_ARM_SMALL_KEYS_ROAM) ==
                      RSBS_COMBO_COMPOSE_SURPLUS,
              "C1: a confined family joins the bag exactly when its setting hands it to the general pass");
    CL_ASSERT(Combo_Logic_ComposeDisposition(pool[5].item, 0u, 0xFFFFFFFFu) == RSBS_COMBO_COMPOSE_TRAP,
              "C1: a trap is a trap under every armed word");

    // C2. ARMING, per ORIGIN: keysanity arms OoT's key; the souls cap arms MM's
    //     soul row; neither word arms the other game's row.
    req.armedOoT = RSBS_FILL_ARM_SMALL_KEYS_ROAM;
    req.armedMM = RSBS_FILL_ARM_SOULS;
    CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_OK && res.bagCount == 9 &&
                  res.perGame[O].rows[RSBS_COMBO_COMPOSE_CONFINED] == 0 &&
                  res.perGame[M].rows[RSBS_COMBO_COMPOSE_CONFINED] == 0,
              "C2: armed families enter the bag");
    req.armedOoT = RSBS_FILL_ARM_SOULS;
    req.armedMM = RSBS_FILL_ARM_SMALL_KEYS_ROAM;
    CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_OK && res.bagCount == 7,
              "C2: the armed word is per ORIGIN - one game's settings never arm the other's rows");
    req.armedOoT = 0u;
    req.armedMM = 0u;

    // C3. CAPACITY: refused, counts complete, nothing past the cap written.
    ComboLogicBagItem capped[3];
    memset(capped, 0xAB, sizeof(capped));
    CL_ASSERT(Combo_Logic_ComposeBag(&req, capped, 2, nullptr, &res) == RSBS_COMBO_LOGIC_ERR_CAPACITY &&
                  res.bagCount == 7,
              "C3: more admitted rows than the output holds is REFUSED, and bagCount still says how many");
    CL_ASSERT(capped[2].item.id == 0xABABu, "C3: nothing is written past the caller's capacity");

    // C4. REFUSALS: an unclassified row, an unknown pool flag, a row with no
    //     origin — each refuses the whole compose, and each is still COUNTED
    //     exactly once (the header's "counts are complete on every refusal").
    ComboLogicPoolRow bad[3] = { ClPoolRow(O, 1, 0), ClPoolRow(O, 9, 0), ClPoolRow(M, 1, 0) };
    req.rows = bad;
    req.rowCount = 3;
    CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST &&
                  res.perGame[O].rows[RSBS_COMBO_COMPOSE_UNCLASSIFIED] == 1,
              "C4: a pool row the owner has no class for refuses the compose (the bag would be a guess)");
    bad[1] = ClPoolRow(O, 1, 0x8000u);
    CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
              "C4: an unknown pool flag is refused, never read as a REQUIRED copy");
    {
        int sumO = 0;
        for (int d = 0; d < RSBS_COMBO_COMPOSE_COUNT; ++d) {
            sumO += res.perGame[O].rows[d];
        }
        CL_ASSERT(sumO == 2 && res.perGame[O].rows[RSBS_COMBO_COMPOSE_UNCLASSIFIED] == 1 && res.noOrigin == 0,
                  "C4: the unknown-flag row is counted once, under UNCLASSIFIED, so the counts still cover the pool");
    }
    bad[1] = ClPoolRow((uint8_t)GAME_NONE, 1, 0);
    CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
              "C4: a row with no origin game is refused");
    {
        int sum = 0;
        for (int d = 0; d < RSBS_COMBO_COMPOSE_COUNT; ++d) {
            sum += res.perGame[O].rows[d] + res.perGame[M].rows[d];
        }
        CL_ASSERT(res.noOrigin == 1 && sum + res.noOrigin == 3,
                  "C4: the no-origin row is counted in noOrigin, so per-game counts plus noOrigin cover the pool");
    }
    CL_ASSERT(Combo_Logic_ComposeBag(nullptr, bag, 16, index, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
              "C4: a NULL request is refused");

    printf("[TEST] combo-logic-bag-model: C composition rule: 14 pool rows -> 7 bag rows (5 required, 2 surplus); "
           "confined, filler and trap rows counted per game and never admitted\n");
    return TEST_PASS;
}

// ---------------------------------------------------------------------------
// P. THE PRODUCTION RULES (ABI 5; #645 lane K11): the four things the single-bag
//    wiring needs from the coordinator, locked over the stub engines — HOME_ONLY
//    rows, the per-engine foreign-host predicate, the fill observer's stop, and
//    CONFINED rows admitted home-only. Each leg carries its counterfactual, so a
//    rule that stopped applying would go red rather than pass on a lucky draw.
// ---------------------------------------------------------------------------
int ClHostOnlyAccepts41(void* self, uint16_t hostCheck) {
    (void)self;
    return hostCheck == 41 ? 1 : 0;
}

int ClHostAcceptsNone(void* self, uint16_t hostCheck) {
    (void)self;
    (void)hostCheck;
    return 0;
}

struct ClObserveCounter {
    int calls = 0;
    int stopAt = -1; // call index that returns "stop"; -1 never
    int lastStage = -1;
};

int ClObserve(void* ctx, const ComboLogicFillProgress* p) {
    ClObserveCounter* c = (ClObserveCounter*)ctx;
    const int index = c->calls++;
    c->lastStage = p->stage;
    return (c->stopAt >= 0 && index >= c->stopAt) ? 1 : 0;
}

/** Placements whose item is NOT of the host's own game, per host game. */
void ClCountCrossings(int* intoOoT, int* intoMM, bool* ootForeignOnly41) {
    ComboLogicPlacement p;
    *intoOoT = 0;
    *intoMM = 0;
    if (ootForeignOnly41 != nullptr) {
        *ootForeignOnly41 = true;
    }
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
        if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame != (uint8_t)GAME_OOT) {
            (*intoOoT)++;
        }
    }
    for (int i = 0; i < Combo_Logic_PlacementCount(GAME_MM); ++i) {
        if (Combo_Logic_PlacementAt(GAME_MM, i, &p) && p.item.originGame != (uint8_t)GAME_MM) {
            (*intoMM)++;
            if (ootForeignOnly41 != nullptr && p.hostCheck != 41) {
                *ootForeignOnly41 = false;
            }
        }
    }
}

TestResult ClProductionRulesLeg(void) {
    CL_ASSERT(RSBS_COMBO_LOGIC_ENGINE_ABI == 11u,
              "P0: this build speaks ABI 11 (5: the production wiring's hostAcceptsForeign; 6: the round's forbidden "
              "hosts; 7: the shared pair; 8: All Locations' allLocations; 9: the host groups; 10: travel facts; 11: "
              "the combo-wide groups)");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    const uint16_t oItems[4] = { kOotSword, kOotHook, kOotLens, kOotBoots };
    const uint16_t mItems[4] = { kMmOcarina, kMmBow, kMmRemains, kMmMask };

    // P1. HOME_ONLY: an exact-fit world of 4 + 4 hosts and 4 + 4 rows. Home-only,
    //     every row lands on its own game on every seed; without the flag, some
    //     seed crosses (the draw CAN cross, so the flag is what stopped it).
    {
        ComboLogicBagItem home[8];
        ComboLogicBagItem crossable[8];
        for (int i = 0; i < 4; ++i) {
            home[i] = ClBagItem(O, oItems[i], RSBS_ITEMCLASS_PROGRESSION);
            home[4 + i] = ClBagItem(M, mItems[i], RSBS_ITEMCLASS_PROGRESSION);
            home[i].bagFlags = RSBS_COMBO_BAG_HOME_ONLY;
            home[4 + i].bagFlags = RSBS_COMBO_BAG_HOME_ONLY;
            crossable[i] = ClBagItem(O, oItems[i], RSBS_ITEMCLASS_PROGRESSION);
            crossable[4 + i] = ClBagItem(M, mItems[i], RSBS_ITEMCLASS_PROGRESSION);
        }
        int freeCrossed = 0;
        for (uint32_t seed = 1; seed <= 16; ++seed) {
            for (const uint8_t rung : { (uint8_t)RSBS_COMBO_RUNG_NONE, (uint8_t)RSBS_COMBO_RUNG_BEATABLE }) {
                ClBuildOpenWorld(4, 4);
                ComboLogicFillResult res;
                CL_ASSERT(ClRunFill(home, 8, RSBS_COMBO_GOAL_BEAT_EITHER, rung, seed, &res) == RSBS_COMBO_LOGIC_OK,
                          "P1: a home-only exact fit fills");
                int intoOoT = 0;
                int intoMM = 0;
                ClCountCrossings(&intoOoT, &intoMM, nullptr);
                CL_ASSERT(intoOoT == 0 && intoMM == 0, "P1: a HOME_ONLY row never lands on the other game's host");
                ClBuildOpenWorld(4, 4);
                CL_ASSERT(ClRunFill(crossable, 8, RSBS_COMBO_GOAL_BEAT_EITHER, rung, seed, &res) == RSBS_COMBO_LOGIC_OK,
                          "P1: the same rows without the flag fill");
                ClCountCrossings(&intoOoT, &intoMM, nullptr);
                freeCrossed += (intoOoT + intoMM > 0) ? 1 : 0;
            }
        }
        CL_ASSERT(freeCrossed > 0, "P1: without HOME_ONLY no seed crossed, so the leg above proves nothing");
    }

    // P1b. THE HOME-CAPACITY COUNTS (#1483), over the production counter
    //      Combo_SingleBag_CountHomeCapacity: 4 MM HOME_ONLY REQUIRED rows on 3 MM
    //      hosts (OoT roomy, plus one OoT row that may cross) fail the fill, and
    //      after the roll-back the counter names MM with 4 rows and 3 hosts. The
    //      discriminating bags: 3 rows (an exact fit) and 3 rows plus one SURPLUS
    //      HOME_ONLY row (a surplus copy may be dropped) name no game.
    {
        ComboLogicBagItem bag[5];
        for (int i = 0; i < 4; ++i) {
            bag[i] = ClBagItem(M, mItems[i], RSBS_ITEMCLASS_PROGRESSION);
            bag[i].bagFlags = RSBS_COMBO_BAG_HOME_ONLY;
        }
        bag[4] = ClBagItem(O, oItems[0], RSBS_ITEMCLASS_PROGRESSION); // not HOME_ONLY: never counted
        ClBuildOpenWorld(4, 3);
        ComboLogicFillResult res;
        const int rc = ClRunFill(bag, 5, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_NONE, 1u, &res);
        CL_ASSERT(rc == RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE, "P1b: 4 MM HOME_ONLY rows on 3 MM hosts cannot fill");
        Combo_Logic_ResetPlacements();
        ComboSingleBagReport r;
        memset(&r, 0, sizeof(r));
        r.status = rc;
        Combo_SingleBag_CountHomeCapacity(bag, 5, &r);
        // Catches: the origin tests swapped (OoT 4), a row that is not HOME_ONLY
        // counted (OoT 1), the hosts read from the wrong game (MM 4). That
        // Combo_SingleBag_Run calls the counter AFTER its roll-back needs both real
        // engines and is not locked here.
        CL_ASSERT(r.homeRequiredMM == 4 && r.homeRequiredOoT == 0,
                  "P1b: the counter counts the HOME_ONLY REQUIRED rows by origin, and only those");
        CL_ASSERT(r.hostsMM == 3 && r.hostsOoT == 4, "P1b: after the roll-back every empty host of each game counts");
        CL_ASSERT(Combo_SingleBag_HomeShortGames(&r) == RSBS_COMBO_HALF_MM,
                  "P1b: MM, and only MM, is named short (OoT has room)");

        ClBuildOpenWorld(4, 3);
        memset(&r, 0, sizeof(r));
        r.status = RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE;
        Combo_SingleBag_CountHomeCapacity(bag, 3, &r);
        CL_ASSERT(r.homeRequiredMM == 3 && r.hostsMM == 3 && Combo_SingleBag_HomeShortGames(&r) == 0u,
                  "P1b: 3 HOME_ONLY rows on 3 hosts fit: no game is named (catches > turned into >=)");

        bag[3].bagFlags = (uint16_t)(RSBS_COMBO_BAG_HOME_ONLY | RSBS_COMBO_BAG_SURPLUS);
        memset(&r, 0, sizeof(r));
        r.status = RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE;
        Combo_SingleBag_CountHomeCapacity(bag, 4, &r);
        CL_ASSERT(r.homeRequiredMM == 3 && Combo_SingleBag_HomeShortGames(&r) == 0u,
                  "P1b: a SURPLUS HOME_ONLY row is never counted (it may be dropped), so 3 + 1 surplus fit");
    }

    // P2. THE FOREIGN-HOST PREDICATE: MM accepts a foreign item only on host 41,
    //     OoT accepts none. Every crossing into MM is on 41 and none enters OoT;
    //     with the predicates absent, some seed puts an OoT row on another MM host.
    {
        ComboLogicBagItem bag[6];
        for (int i = 0; i < 4; ++i) {
            bag[i] = ClBagItem(O, oItems[i], RSBS_ITEMCLASS_PROGRESSION);
        }
        bag[4] = ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION);
        bag[5] = ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION);
        int unfilteredElsewhere = 0;
        int filteredCrossings = 0;
        for (uint32_t seed = 1; seed <= 16; ++seed) {
            ClBuildOpenWorld(4, 4);
            gClVtMM.hostAcceptsForeign = ClHostOnlyAccepts41;
            gClVtOoT.hostAcceptsForeign = ClHostAcceptsNone;
            CL_ASSERT(Combo_Logic_RegisterEngine(GAME_OOT, &gClVtOoT) && Combo_Logic_RegisterEngine(GAME_MM, &gClVtMM),
                      "P2: engines with a predicate register (the pointer is optional)");
            ComboLogicFillResult res;
            CL_ASSERT(ClRunFill(bag, 6, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_NONE, seed, &res) ==
                          RSBS_COMBO_LOGIC_OK,
                      "P2: the filtered fill places the bag");
            int intoOoT = 0;
            int intoMM = 0;
            bool only41 = true;
            ClCountCrossings(&intoOoT, &intoMM, &only41);
            CL_ASSERT(intoOoT == 0, "P2: no MM row landed on an OoT host that accepts no foreign item");
            CL_ASSERT(only41 && intoMM <= 1, "P2: an OoT row landed on an MM host the predicate refuses");
            filteredCrossings += intoMM;

            ClBuildOpenWorld(4, 4); // predicates cleared by the rebuild
            CL_ASSERT(ClRunFill(bag, 6, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_NONE, seed, &res) ==
                          RSBS_COMBO_LOGIC_OK,
                      "P2: the unfiltered fill places the bag");
            ClCountCrossings(&intoOoT, &intoMM, &only41);
            unfilteredElsewhere += only41 ? 0 : 1;
        }
        CL_ASSERT(filteredCrossings > 0, "P2: host 41 never received a crossing, so the predicate was never consulted");
        CL_ASSERT(unfilteredElsewhere > 0, "P2: without the predicate no OoT row ever landed off host 41 — vacuous");
    }

    // P3. THE OBSERVER: it sees every round and the proof, returning 0 changes
    //     nothing, and returning nonzero stops the fill with ERR_ABORTED — its own
    //     status, not a dead end the ladder would climb on.
    {
        ComboLogicBagItem bag[4];
        for (int i = 0; i < 4; ++i) {
            bag[i] = ClBagItem(O, oItems[i], RSBS_ITEMCLASS_PROGRESSION);
        }
        ClBuildOpenWorld(4, 4);
        ComboLogicFillResult plain;
        CL_ASSERT(ClRunFill(bag, 4, RSBS_COMBO_GOAL_BEAT_EITHER, RSBS_COMBO_RUNG_BEATABLE, 77u, &plain) ==
                      RSBS_COMBO_LOGIC_OK,
                  "P3: the fill with no observer");

        ClBuildOpenWorld(4, 4);
        ClObserveCounter watch;
        ComboLogicFillRequest req;
        memset(&req, 0, sizeof(req));
        req.bag = bag;
        req.bagCount = 4;
        req.goal = RSBS_COMBO_GOAL_BEAT_EITHER;
        req.logicRung = RSBS_COMBO_RUNG_BEATABLE;
        req.seed = 77u;
        req.observer = ClObserve;
        req.observerCtx = &watch;
        ComboLogicFillResult watched;
        CL_ASSERT(Combo_Logic_RunFill(&req, &watched) == RSBS_COMBO_LOGIC_OK, "P3: the observed fill");
        CL_ASSERT(watched.placementDigest == plain.placementDigest && watched.rounds == plain.rounds,
                  "P3: an observer that never stops changes nothing about the world");
        CL_ASSERT(watch.calls == plain.rounds && watch.lastStage == RSBS_COMBO_FILL_STAGE_PROOF,
                  "P3: the observer is told before every round, the exit round last");

        ClBuildOpenWorld(4, 4);
        ClObserveCounter stop;
        stop.stopAt = 2;
        req.observerCtx = &stop;
        ComboLogicFillResult stopped;
        CL_ASSERT(Combo_Logic_RunFill(&req, &stopped) == RSBS_COMBO_LOGIC_ERR_ABORTED,
                  "P3: an observer that asks to stop ends the fill with ERR_ABORTED");
        CL_ASSERT(stopped.attempts == 1 && stopped.rounds == 2 && stop.calls == 3,
                  "P3: the stop is taken where it was asked, inside the first batch, and nothing is retried");
        CL_ASSERT(strcmp(Combo_Logic_StatusName(RSBS_COMBO_LOGIC_ERR_ABORTED), "aborted") == 0,
                  "P3: the stop has its own name");
        Combo_Logic_ResetPlacements();
    }

    // P4. CONFINED ROWS, ADMITTED HOME-ONLY: the composition rule's production
    //     flag. The same pool as leg C; the two confined rows (OoT id 2 behind the
    //     keys bit, MM id 5 behind the souls bit) now enter the bag HOME_ONLY and
    //     are still COUNTED confined. An unknown compose flag is refused.
    {
        const ClComposeSourceGuard guard;
        CL_ASSERT(guard.installed, "P4: both synthetic classification sources installed");
        const uint16_t P = RSBS_COMBO_POOL_PLENTIFUL;
        const ComboLogicPoolRow pool[] = {
            ClPoolRow(O, 1, 0), ClPoolRow(O, 1, P), ClPoolRow(O, 2, 0), ClPoolRow(O, 3, 0), ClPoolRow(O, 4, P),
            ClPoolRow(O, 5, 0), ClPoolRow(M, 1, 0), ClPoolRow(M, 2, 0), ClPoolRow(M, 2, P), ClPoolRow(M, 3, 0),
            ClPoolRow(M, 4, 0), ClPoolRow(M, 5, 0), ClPoolRow(O, 1, 0), ClPoolRow(O, 6, 0),
        };
        ComboLogicComposeRequest req;
        memset(&req, 0, sizeof(req));
        req.rows = pool;
        req.rowCount = (int)(sizeof(pool) / sizeof(pool[0]));
        req.composeFlags = RSBS_COMBO_COMPOSE_ADMIT_CONFINED_HOME;
        // THE SHARED-QUANTITY TRIM refuses an unpublished starting bar (as leg C
        // says); no row here is a trim family, so the values change nothing.
        req.startingHealthOoT = 0x30u;
        req.startingHealthMM = 0x30u;
        ComboLogicBagItem bag[16];
        int index[16];
        ComboLogicComposeResult res;
        CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_OK && res.bagCount == 9,
                  "P4: the two confined rows join the seven of leg C");
        int homeOnly = 0;
        for (int i = 0; i < res.bagCount; ++i) {
            const bool confined = (index[i] == 2 || index[i] == 11);
            CL_ASSERT(((bag[i].bagFlags & RSBS_COMBO_BAG_HOME_ONLY) != 0u) == confined,
                      "P4: exactly the confined rows are HOME_ONLY");
            homeOnly += confined ? 1 : 0;
        }
        CL_ASSERT(homeOnly == 2 && res.perGame[O].rows[RSBS_COMBO_COMPOSE_CONFINED] == 1 &&
                      res.perGame[M].rows[RSBS_COMBO_COMPOSE_CONFINED] == 1,
                  "P4: admitted rows are still counted CONFINED");
        req.composeFlags = 0x8000u;
        CL_ASSERT(Combo_Logic_ComposeBag(&req, bag, 16, index, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "P4: an unknown compose flag is refused");
    }

    // P5. THE PER-SIDE CROSSING BOUND (PR #743 review): four OoT rows on an open
    //     world of 4 OoT + 4 MM hosts. With `maxCrossingsPerSide` = 1, no seed puts
    //     more than one OoT item on an MM host, on either fill path; without the
    //     bound, some seed puts two or more there (the bound is what stopped it).
    {
        ComboLogicBagItem bag[4];
        for (int i = 0; i < 4; ++i) {
            bag[i] = ClBagItem(O, oItems[i], RSBS_ITEMCLASS_PROGRESSION);
        }
        int unboundedOver = 0;
        int boundedAtCap = 0;
        for (uint32_t seed = 1; seed <= 24; ++seed) {
            for (const uint8_t rung : { (uint8_t)RSBS_COMBO_RUNG_NONE, (uint8_t)RSBS_COMBO_RUNG_BEATABLE }) {
                ComboLogicFillRequest req;
                memset(&req, 0, sizeof(req));
                req.bag = bag;
                req.bagCount = 4;
                req.goal = RSBS_COMBO_GOAL_BEAT_EITHER;
                req.logicRung = rung;
                req.seed = seed;
                req.maxCrossingsPerSide = 1;
                ComboLogicFillResult res;
                ClBuildOpenWorld(4, 4);
                CL_ASSERT(Combo_Logic_RunFill(&req, &res) == RSBS_COMBO_LOGIC_OK,
                          "P5: the bounded fill places the bag");
                int intoOoT = 0;
                int intoMM = 0;
                ClCountCrossings(&intoOoT, &intoMM, nullptr);
                CL_ASSERT(intoMM <= 1, "P5: a side received more crossings than the per-side bound allows");
                boundedAtCap += (intoMM == 1) ? 1 : 0;

                req.maxCrossingsPerSide = 0;
                ClBuildOpenWorld(4, 4);
                CL_ASSERT(Combo_Logic_RunFill(&req, &res) == RSBS_COMBO_LOGIC_OK, "P5: the unbounded fill");
                ClCountCrossings(&intoOoT, &intoMM, nullptr);
                unboundedOver += (intoMM > 1) ? 1 : 0;
            }
        }
        CL_ASSERT(boundedAtCap > 0, "P5: the bounded fill never crossed at all, so the bound was never reached");
        CL_ASSERT(unboundedOver > 0, "P5: without the bound no seed crossed twice, so the leg proves nothing");
        Combo_Logic_ResetPlacements();
    }

    // P6. THE PRODUCTION BOUND LETS A SIDE HOST MORE CROSSINGS THAN THE FIXED
    //     SHARED-ITEM SEGMENT (operator ruling 2026-10-27; Refs #1222). 80 OoT rows
    //     on a world of 4 OoT + 100 MM hosts, under RSBS_CROSSINGS_PER_SIDE_MAX, the
    //     bound the single bag passes (combo_single_bag.c): at least 76 OoT items
    //     must land on MM hosts. While the bound was the 64-slot array, the 65th
    //     crossing found its side full and the bag could not be placed.
    {
        const int kRows = (int)RSBS_SHARED_ITEM_CAP + 16;
        const int kOoTHosts = 4;
        const int kMMHosts = kRows + 20;
        static_assert((int)RSBS_SHARED_ITEM_CAP + 16 - 4 > (int)RSBS_SHARED_ITEM_CAP,
                      "P6 must need more crossings on one side than the fixed segment holds");
        static_assert((int)RSBS_SHARED_ITEM_CAP + 36 <= kClMaxPlaced, "P6's MM engine must store every placement");
        ComboLogicBagItem bag[(int)RSBS_SHARED_ITEM_CAP + 16];
        for (int i = 0; i < kRows; ++i) {
            bag[i] = ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
        }
        ComboLogicFillRequest req;
        memset(&req, 0, sizeof(req));
        req.bag = bag;
        req.bagCount = kRows;
        req.goal = RSBS_COMBO_GOAL_BEAT_BOTH;
        req.logicRung = RSBS_COMBO_RUNG_NONE;
        req.seed = 0x1222u;
        req.maxCrossingsPerSide = (uint16_t)RSBS_CROSSINGS_PER_SIDE_MAX;
        ComboLogicFillResult res;
        ClBuildWideHostWorld(kOoTHosts, kMMHosts);
        const int status = Combo_Logic_RunFill(&req, &res);
        int intoOoT = 0;
        int intoMM = 0;
        ClCountCrossings(&intoOoT, &intoMM, nullptr);
        printf("[TEST]   P6: bound %u, %d OoT rows on %d OoT + %d MM hosts: %s, %d placed, %d crossings into MM\n",
               (unsigned)RSBS_CROSSINGS_PER_SIDE_MAX, kRows, kOoTHosts, kMMHosts, Combo_Logic_StatusName(status),
               res.placed, intoMM);
        CL_ASSERT(status == RSBS_COMBO_LOGIC_OK && res.placed == kRows,
                  "P6: the production per-side bound refused a bag that needs more crossings on one side than the "
                  "fixed 64 shared-item slots");
        CL_ASSERT(intoMM >= kRows - kOoTHosts && intoMM > (int)RSBS_SHARED_ITEM_CAP,
                  "P6: MM did not host more crossings than the fixed segment holds");
        Combo_Logic_ResetPlacements();
    }

    printf("[TEST] combo-logic-bag-model: P production rules: HOME_ONLY rows stay home, the foreign-host predicate "
           "filters crossings, the observer stops with ERR_ABORTED and otherwise changes nothing, confined rows enter "
           "home-only, the per-side crossing bound holds\n");
    return TEST_PASS;
}
} // namespace

TestResult Test_ComboLogicBagModel(void) {
    printf("[TEST] combo-logic-bag-model: one assume per COPY, and the surplus / filler / exact-fit rules of the "
           "bag (the 2026-09-26 multiplicity ruling)\n");
    const uint8_t kRungs[2] = { RSBS_COMBO_RUNG_BEATABLE, RSBS_COMBO_RUNG_NONE };

    // ------------------------------------------------------------------
    // M1. ONE CALL PER COPY, COUNTED — and count-sensitive, both ways.
    // ------------------------------------------------------------------
    {
        ClBuildOpenWorld(2, 2);
        gClOoT.hostNeedItem[0] = kOotHook; // host 10 needs THREE hookshots
        gClOoT.hostNeedCount[0] = 3;
        ClInstall();

        ComboLogicBagItem three[3] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, RSBS_ITEMCLASS_PROGRESSION),
                                       ClBagItem((uint8_t)GAME_OOT, kOotHook, RSBS_ITEMCLASS_PROGRESSION),
                                       ClBagItem((uint8_t)GAME_OOT, kOotHook, RSBS_ITEMCLASS_PROGRESSION) };
        ComboLogicRoundResult r;
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, three, 3, &r) == RSBS_COMBO_LOGIC_OK, "three-copy round");
        CL_ASSERT(ClCallsFor(&gClOoT, kOotHook) == 3,
                  "three rows of one id must be THREE assumeOwnItem calls — the coordinator passes every copy");
        CL_ASSERT(r.candidatesOoT == 2, "with three copies assumed, the host that needs three is reached");

        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, three, 2, &r) == RSBS_COMBO_LOGIC_OK, "two-copy round");
        CL_ASSERT(ClCallsFor(&gClOoT, kOotHook) == 5, "and two rows are two more calls, not one");
        CL_ASSERT(r.candidatesOoT == 1,
                  "with only two copies the three-copy host is NOT reached — the count reaches the engine, so the "
                  "round is sensitive to it (the sensitivity control for the leg above)");

        // THE EXCHANGE delivers each placement once per round, however many
        // alternations run: two MM-hosted copies plus one assumed copy is three.
        CL_ASSERT(Combo_Logic_Place(GAME_MM, 40, ClItem((uint8_t)GAME_OOT, kOotHook), RSBS_ITEMCLASS_PROGRESSION),
                  "author an OoT hookshot on MM host 40");
        CL_ASSERT(Combo_Logic_Place(GAME_MM, 41, ClItem((uint8_t)GAME_OOT, kOotHook), RSBS_ITEMCLASS_PROGRESSION),
                  "and another on MM host 41");
        const int before = ClCallsFor(&gClOoT, kOotHook);
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_BOTH, three, 1, &r) == RSBS_COMBO_LOGIC_OK, "exchange round");
        CL_ASSERT(r.exchanged == 2, "both MM-hosted copies cross, one exchange each");
        CL_ASSERT(r.iterations >= 2, "the round alternated more than once, so a re-delivery had its chance");
        CL_ASSERT(ClCallsFor(&gClOoT, kOotHook) - before == 3,
                  "one seeded copy plus two exchanged copies must be exactly THREE calls — a later alternation must "
                  "never re-deliver a copy it already handed over");
        CL_ASSERT(r.candidatesOoT == 2, "and three copies across two sources reach the three-copy host");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // ------------------------------------------------------------------
    // M2. THE COORDINATOR PASSES THE MULTISET THROUGH IN EITHER ORDER. The stub
    //     counts copies commutatively by construction, so this leg cannot show
    //     that an ENGINE is order-independent (combo-logic-multiplicity's P legs
    //     do that over the real engines, red half observed). What it guards is
    //     that the coordinator neither drops nor duplicates a copy depending on
    //     where in the assumed set the copy sits.
    // ------------------------------------------------------------------
    {
        ClBuildOpenWorld(3, 1);
        gClOoT.hostNeedItem[0] = kOotHook;
        gClOoT.hostNeedCount[0] = 2;
        gClOoT.hostRequires[1] = ClBit(&gClOoT, kOotSword);
        gClOoT.goalNeedItem = kOotHook;
        gClOoT.goalNeedCount = 3;
        ClInstall();
        const ComboLogicBagItem a[4] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                         ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                         ClBagItem((uint8_t)GAME_OOT, kOotSword, 0),
                                         ClBagItem((uint8_t)GAME_OOT, kOotHook, 0) };
        const ComboLogicBagItem b[4] = { a[2], a[0], a[3], a[1] };
        ComboLogicRoundResult ra;
        ComboLogicRoundResult rb;
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, a, 4, &ra) == RSBS_COMBO_LOGIC_OK, "order A");
        CL_ASSERT(ClRunRound(RSBS_COMBO_GOAL_BEAT_EITHER, b, 4, &rb) == RSBS_COMBO_LOGIC_OK, "order B");
        CL_ASSERT(ra.candidatesOoT == rb.candidatesOoT && ra.goalOoT == rb.goalOoT &&
                      ra.goalExpression == rb.goalExpression,
                  "the same multiset of copies in a different order gave different facts");
        CL_ASSERT(ra.goalOoT == 1 && ra.candidatesOoT == 3, "and those facts are the three-copy ones, not one copy's");
    }

    // ------------------------------------------------------------------
    // M3. THE K3 SHAPE, OVER A COUNTING STUB: a world whose goal needs THREE
    //     copies of one id is PROVED from a bag of three rows and not from two —
    //     the fill carries every copy to the engine. (The de-dup that made the
    //     real worlds unprovable was engine-side; see the note above.)
    // ------------------------------------------------------------------
    for (int r = 0; r < 1; ++r) {
        ClBuildOpenWorld(4, 4);
        gClOoT.goalNeedItem = kOotHook;
        gClOoT.goalNeedCount = 3;
        ClInstall();
        const ComboLogicBagItem bag[3] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                           ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                           ClBagItem((uint8_t)GAME_OOT, kOotHook, 0) };
        ComboLogicFillResult res;
        CL_ASSERT(ClRunFill(bag, 3, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x1234u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "three copies placed on reached hosts must PROVE a goal that needs three — every placed copy is "
                  "harvested once per host");
        CL_ASSERT(res.goalProven && res.requiredPlaced == 3, "proved, with all three required copies placed");
        // Two copies cannot prove it: the proof is really counting.
        CL_ASSERT(ClRunFill(bag, 2, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x1234u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "two copies must NOT prove a goal that needs three");
    }

    // ------------------------------------------------------------------
    // S-a. MORE HOSTS THAN BAG: the coordinator places the bag and nothing else;
    //      the rest is LEFTOVER, for each game's own per-game pass (traps, junk).
    // ------------------------------------------------------------------
    for (int ri = 0; ri < 2; ++ri) {
        ClBuildOpenWorld(6, 6);
        const ComboLogicBagItem bag[3] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                           ClBagItem((uint8_t)GAME_MM, kMmBow, 0),
                                           ClBagItem((uint8_t)GAME_OOT, kOotHook, 0) };
        ComboLogicFillResult res;
        CL_ASSERT(ClRunFill(bag, 3, RSBS_COMBO_GOAL_BEAT_BOTH, kRungs[ri], 0xABCDu, &res) == RSBS_COMBO_LOGIC_OK,
                  "a small bag in a big world must fill");
        CL_ASSERT(res.placed == 3 && res.requiredPlaced == 3 && res.surplusPlaced == 0 && res.surplusDropped == 0,
                  "exactly the bag was placed — three rows, no filler invented");
        CL_ASSERT(res.leftoverHostsOoT + res.leftoverHostsMM == 12 - 3,
                  "every host no row landed on is LEFTOVER, and counted");
        CL_ASSERT(ClLeftoverConsistent(GAME_OOT, res.leftoverHostsOoT) &&
                      ClLeftoverConsistent(GAME_MM, res.leftoverHostsMM),
                  "the leftover list is ascending, matches the count and never names a placed host");
        CL_ASSERT(ClPlacementsAreBagRows(bag, 3), "every placement is a bag row: no junk and no trap is ever placed by "
                                                  "the coordinator, which invents no filler of its own");
        CL_ASSERT(Combo_Logic_SurplusDroppedCount() == 0, "nothing dropped");
        CL_ASSERT(ClContractClean(), "the contract traps must stay clear");
    }

    // ------------------------------------------------------------------
    // S-b. MORE BAG THAN HOSTS: surplus copies dropped LAST-FIRST, never a
    //      required one, deterministically.
    // ------------------------------------------------------------------
    for (int ri = 0; ri < 2; ++ri) {
        ClBuildOpenWorld(2, 2);
        // Two required rows and four surplus rows over four hosts. Distinct ids on
        // the surplus rows so WHICH were dropped is legible.
        const ComboLogicBagItem bag[6] = {
            ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),     ClSurplusItem((uint8_t)GAME_OOT, kOotSword, 0),
            ClBagItem((uint8_t)GAME_MM, kMmBow, 0),        ClSurplusItem((uint8_t)GAME_MM, kMmMask, 0),
            ClSurplusItem((uint8_t)GAME_OOT, kOotLens, 0), ClSurplusItem((uint8_t)GAME_MM, kMmRemains, 0),
        };
        ComboLogicFillResult res;
        ComboLogicFillResult again;
        CL_ASSERT(ClRunFill(bag, 6, RSBS_COMBO_GOAL_BEAT_BOTH, kRungs[ri], 0x5EEDu, &res) == RSBS_COMBO_LOGIC_OK,
                  "a plentiful surplus is not a failure: it is dropped");
        CL_ASSERT(res.placed == 4 && res.requiredPlaced == 2 && res.surplusPlaced == 2 && res.surplusDropped == 2,
                  "four hosts hold both required rows and the first two surplus rows; two surplus rows are dropped");
        int d0 = -1;
        int d1 = -1;
        CL_ASSERT(Combo_Logic_SurplusDroppedCount() == 2 && Combo_Logic_SurplusDroppedAt(0, &d0) &&
                      Combo_Logic_SurplusDroppedAt(1, &d1),
                  "the dropped rows are readable back");
        CL_ASSERT(d0 == 4 && d1 == 5,
                  "the dropped rows are the LAST surplus rows in bag order (indices 4 and 5) — last-first");
        CL_ASSERT(!Combo_Logic_SurplusDroppedAt(2, &d0), "and there are exactly two");
        CL_ASSERT(ClAnyTableHolds(kOotHook) && ClAnyTableHolds(kMmBow), "no REQUIRED row is ever dropped");
        CL_ASSERT(res.leftoverHostsOoT + res.leftoverHostsMM == 0, "and the world is full");
        CL_ASSERT(ClRunFill(bag, 6, RSBS_COMBO_GOAL_BEAT_BOTH, kRungs[ri], 0x5EEDu, &again) == RSBS_COMBO_LOGIC_OK,
                  "same seed, again");
        CL_ASSERT(again.placementDigest == res.placementDigest && again.droppedDigest == res.droppedDigest &&
                      again.surplusDropped == res.surplusDropped,
                  "the SAME SEED must give the same placements AND THE SAME DROPS");
        CL_ASSERT(res.droppedDigest != 2166136261u, "and the drop digest is not the empty one (a sensitivity check)");

        // A REQUIRED overflow is a failure, never a drop.
        const ComboLogicBagItem tooMany[5] = {
            ClBagItem((uint8_t)GAME_OOT, kOotHook, 0), ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
            ClBagItem((uint8_t)GAME_OOT, kOotHook, 0), ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
            ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
        };
        CL_ASSERT(ClRunFill(tooMany, 5, RSBS_COMBO_GOAL_BEAT_BOTH, kRungs[ri], 0x5EEDu, &res) ==
                      RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE,
                  "five REQUIRED rows on four hosts is a capacity failure — a required copy is never dropped");
        CL_ASSERT(res.surplusDropped == 0 && Combo_Logic_SurplusDroppedCount() == 0, "and nothing was 'dropped'");
    }

    // ------------------------------------------------------------------
    // S-c. EXACT FIT: nothing dropped, nothing leftover.
    // ------------------------------------------------------------------
    for (int ri = 0; ri < 2; ++ri) {
        ClBuildOpenWorld(2, 2);
        const ComboLogicBagItem bag[4] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                           ClSurplusItem((uint8_t)GAME_OOT, kOotHook, 0),
                                           ClBagItem((uint8_t)GAME_MM, kMmBow, 0),
                                           ClSurplusItem((uint8_t)GAME_MM, kMmBow, 0) };
        ComboLogicFillResult res;
        CL_ASSERT(ClRunFill(bag, 4, RSBS_COMBO_GOAL_BEAT_BOTH, kRungs[ri], 0x77u, &res) == RSBS_COMBO_LOGIC_OK,
                  "an exact fit must fill");
        CL_ASSERT(res.placed == 4 && res.surplusPlaced == 2 && res.surplusDropped == 0 &&
                      res.leftoverHostsOoT + res.leftoverHostsMM == 0,
                  "exact fit: every row placed, nothing dropped, nothing leftover");
        if (kRungs[ri] == RSBS_COMBO_RUNG_BEATABLE) {
            // required rounds + the proof round + the CONFIRMING round the
            // surplus placements trigger.
            CL_ASSERT(res.rounds == 2 + 1 + 1, "the surplus phase ran exactly one confirming round");
        }
    }

    // ------------------------------------------------------------------
    // S-d. SURPLUS IS NEVER LOAD-BEARING: the proof runs with every surplus copy
    //      absent, so a needed copy marked surplus leaves the goal unprovable.
    // ------------------------------------------------------------------
    {
        ClBuildOpenWorld(4, 4);
        gClOoT.goalNeedItem = kOotHook;
        gClOoT.goalNeedCount = 2;
        ClInstall();
        const ComboLogicBagItem oneSurplus[2] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                                  ClSurplusItem((uint8_t)GAME_OOT, kOotHook, 0) };
        const ComboLogicBagItem bothRequired[2] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),
                                                    ClBagItem((uint8_t)GAME_OOT, kOotHook, 0) };
        ComboLogicFillResult res;
        CL_ASSERT(ClRunFill(oneSurplus, 2, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 9u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE,
                  "a goal that needs two copies must NOT be proved from one required and one SURPLUS copy — the "
                  "proof may not lean on a droppable row");
        CL_ASSERT(ClRunFill(bothRequired, 2, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 9u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "and the same two copies both REQUIRED prove it (the control)");
    }

    // ------------------------------------------------------------------
    // S-e. THE SURPLUS HOST SOURCE IS THE RUNG'S: reached hosts under
    //      `all-reachable` (a surplus copy on an unreached host would break that
    //      rung's own promise), EVERY empty host under `beatable` and `none` (a
    //      surplus copy is not load-bearing, so an unreached host is harmless —
    //      and both native fills size plentiful against ALL empty locations).
    // ------------------------------------------------------------------
    {
        ClBuildUnreachedOnlyWorld(); // OoT host 10 is never reached
        ClAddHost(&gClOoT, 11, 0u);  // ... and host 11 always is
        ClInstall();
        const ComboLogicBagItem bag[3] = { ClBagItem((uint8_t)GAME_OOT, kOotSword, 0),
                                           ClSurplusItem((uint8_t)GAME_OOT, kOotSword, 0),
                                           ClSurplusItem((uint8_t)GAME_OOT, kOotSword, 0) };
        ComboLogicFillResult res;
        // ABI 8: host 10 is a LOCATION no seed reaches, so All Locations refuses this
        // world outright (the red half) ...
        CL_ASSERT(ClRunFill(bag, 3, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 3u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE,
                  "all-reachable refuses a world whose location 10 no seed reaches");
        // ... and with host 10 CURATED out of the location list (OoTMM's
        // removeLocations), the rule under test here applies: it is still a host,
        // and the surplus may not go there.
        gClOoT.withAllLocations = true;
        gClOoT.notLocation = 1u << 0;
        ClInstall();
        CL_ASSERT(ClRunFill(bag, 3, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_ALL_REACHABLE, 3u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "all-reachable with surplus must succeed");
        CL_ASSERT(res.allLocationsReached && res.locationsChecked == 1,
                  "every location (host 11; host 10 curated out) was reached");
        CL_ASSERT(res.requiredPlaced == 1 && res.surplusPlaced == 0 && res.surplusDropped == 2,
                  "the one reached host takes the required row, and the surplus rows are DROPPED rather than put on "
                  "the unreached host");
        CL_ASSERT(!Combo_Logic_GetPlacement(GAME_OOT, 10, NULL), "the unreached host holds nothing from the bag");
        CL_ASSERT(res.leftoverHostsOoT == 1 && res.allHostsReached, "it is LEFTOVER, and every placed host is reached");

        CL_ASSERT(ClRunFill(bag, 3, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_NONE, 3u, &res) == RSBS_COMBO_LOGIC_OK,
                  "`none` makes no reachability claim");
        CL_ASSERT(res.requiredPlaced == 1 && res.surplusPlaced == 1 && res.surplusDropped == 1,
                  "so under `none` a surplus row may take the unreached host, and only the last one is dropped");

        // `beatable`: the REQUIRED row still needs a reached host (the proof is
        // over required rows), but a surplus row draws from every empty host.
        CL_ASSERT(ClRunFill(bag, 3, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 3u, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "beatable with surplus must succeed");
        CL_ASSERT(res.goalProven, "and it is proved, surplus absent and then confirmed present");
        CL_ASSERT(Combo_Logic_GetPlacement(GAME_OOT, 11, NULL), "the required row took the one reached host");
        CL_ASSERT(res.requiredPlaced == 1 && res.surplusPlaced == 1 && res.surplusDropped == 1,
                  "under `beatable` a surplus row takes the UNREACHED empty host rather than being dropped while it "
                  "is free — only the row that finds no empty host at all is dropped");
        CL_ASSERT(Combo_Logic_GetPlacement(GAME_OOT, 10, NULL), "the unreached host holds a surplus copy");
        CL_ASSERT(res.leftoverHostsOoT == 0, "and nothing is left over for the junk pass");
        int dropped = -1;
        CL_ASSERT(Combo_Logic_SurplusDroppedAt(0, &dropped) && dropped == 2,
                  "the dropped row is the LAST surplus row in bag order");
    }

    // ------------------------------------------------------------------
    // S-f. AN UNKNOWN BAG FLAG IS REFUSED, NOT PLACED AS REQUIRED.
    // ------------------------------------------------------------------
    {
        ClBuildOpenWorld(2, 2);
        ComboLogicBagItem bag[1] = { ClBagItem((uint8_t)GAME_OOT, kOotHook, 0) };
        bag[0].bagFlags = 0x8000u;
        ComboLogicFillResult res;
        // First a fill that DROPS, so the drop record has something in it.
        const ComboLogicBagItem dropping[6] = {
            ClBagItem((uint8_t)GAME_OOT, kOotHook, 0),     ClSurplusItem((uint8_t)GAME_OOT, kOotSword, 0),
            ClBagItem((uint8_t)GAME_MM, kMmBow, 0),        ClSurplusItem((uint8_t)GAME_MM, kMmMask, 0),
            ClSurplusItem((uint8_t)GAME_OOT, kOotLens, 0), ClSurplusItem((uint8_t)GAME_MM, kMmRemains, 0),
        };
        CL_ASSERT(ClRunFill(dropping, 6, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 0x5EEDu, &res) ==
                      RSBS_COMBO_LOGIC_OK,
                  "the dropping fill ran");
        CL_ASSERT(res.surplusDropped == 2 && Combo_Logic_SurplusDroppedCount() == 2, "it dropped two rows");
        const int placedBefore = Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM);

        CL_ASSERT(ClRunFill(bag, 1, RSBS_COMBO_GOAL_BEAT_BOTH, RSBS_COMBO_RUNG_BEATABLE, 1u, &res) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "a bag flag this build does not understand must be refused");
        CL_ASSERT(res.attempts == 0 && res.placed == 0, "before any attempt");
        // THE REFUSAL TOUCHES NEITHER RECORD: `res` describes THIS call (nothing
        // dropped), while the tables AND the drop record still describe the
        // attempt that built them — the documented split (combo_logic.h,
        // Combo_Logic_SurplusDroppedCount).
        CL_ASSERT(res.surplusDropped == 0 && res.droppedDigest == 0u, "the refused call reports no drops of its own");
        CL_ASSERT(Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM) == placedBefore,
                  "a pre-attempt refusal left the previous fill's tables in place");
        CL_ASSERT(Combo_Logic_SurplusDroppedCount() == 2,
                  "and it left the previous fill's drop record in place with them, as documented");
    }

    ClUninstall();

    // ------------------------------------------------------------------
    // C. THE BAG COMPOSITION RULE (#645 lane K9): Combo_Logic_ComposeBag over
    //    SYNTHETIC classification sources for both origins, so every row of the
    //    rule table in combo_logic.h is exercised on ids whose class the lock
    //    itself authored — including the shared-kind reconciliation (#731) the
    //    builder reads through Combo_ItemClassOf.
    // ------------------------------------------------------------------
    if (ClComposeLeg() != TEST_PASS) {
        return TEST_FAIL;
    }
    if (ClProductionRulesLeg() != TEST_PASS) {
        ClUninstall();
        return TEST_FAIL;
    }
    ClUninstall();

    printf("[TEST] combo-logic-bag-model: PASS\n");
    return TEST_PASS;
}

// ============================================================================
// THE SHARED PAIR (ABI 7, #976, epic #975 P1)
// ============================================================================

namespace {

// The test-only family: one item each game has, ids disjoint as everywhere in
// this fixture so a misrouted half trips the ADR 0002 trap.
const uint16_t kOotPairMask = 105;
const uint16_t kMmPairMask = 205;
// A crossing requirement no item in these worlds can meet.
const uint32_t kClNeverBit = 1u << 7;

/** The shared-pair bag row: OoT's half as the row's item, MM's id as pairId. */
ComboLogicBagItem ClPairRow() {
    ComboLogicBagItem b = ClBagItem((uint8_t)GAME_OOT, kOotPairMask, RSBS_ITEMCLASS_PROGRESSION);
    b.bagFlags = RSBS_COMBO_BAG_SHARED_PAIR;
    b.pairId = kMmPairMask;
    return b;
}

/**
 * THE PAIR WORLD. Each half's goal needs ITS OWN copy of the family, so a round
 * proves beat-both only if BOTH engines were credited. Each side has one
 * sphere-zero host when `ootHost` / `mmHost` says so; with one side empty the
 * fill has exactly one place for the row, so the scenario is deterministic.
 */
void ClBuildPairWorld(bool ootHost, bool mmHost) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotPairMask);
    if (ootHost) {
        ClAddHost(&gClOoT, 10, 0u);
    }
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = ClBit(&gClOoT, kOotPairMask);

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmPairMask);
    if (mmHost) {
        ClAddHost(&gClMM, 20, 0u);
    }
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = ClBit(&gClMM, kMmPairMask);
    ClInstall();
}

int ClPairAssumeCalls(const ClEngine* e, uint16_t id) {
    const int k = ClBitIndex(e, id);
    return (k >= 0) ? e->assumeCalls[k] : -1;
}

int ClPairCopies(const ClEngine* e, uint16_t id) {
    const int k = ClBitIndex(e, id);
    return (k >= 0) ? (int)e->haveCount[k] : -1;
}

} // namespace

// A SOFT check: every assertion of the row runs even after one fails, so a
// single mutation shows each assertion it breaks (the per-assertion RED).
static int sClpFailures;
#define CLP_CHECK(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            sClpFailures++;                                                 \
        }                                                                   \
    } while (0)

TestResult Test_ComboLogicSharedPair(void) {
    printf("[TEST] combo-logic-shared-pair: one shared-pair bag row is credited by BOTH engines, its far half goes "
           "through the crossing gate, and the composer replaces both games' rows with one (ABI 7, #976)\n");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    sClpFailures = 0;
    ClUninstall();

    // ---- (1) SEED: an assumed shared-pair row is one copy in EACH engine ----
    {
        ClBuildPairWorld(true, true);
        const ComboLogicBagItem assumed[1] = { ClPairRow() };
        ComboLogicRoundResult r;
        const int rc = ClRunRound((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, assumed, 1, &r);
        printf("[TEST] (1) seed: rc %d goalOoT %d goalMM %d, assume calls OoT %d MM %d (expected 0, 1, 1, 1, 1)\n", rc,
               r.goalOoT, r.goalMM, ClPairAssumeCalls(&gClOoT, kOotPairMask), ClPairAssumeCalls(&gClMM, kMmPairMask));
        CLP_CHECK(rc == RSBS_COMBO_LOGIC_OK, "(1) the round runs");
        CLP_CHECK(ClPairAssumeCalls(&gClOoT, kOotPairMask) == 1,
                  "(1) the OoT engine was not credited the row's OoT half exactly once");
        CLP_CHECK(ClPairAssumeCalls(&gClMM, kMmPairMask) == 1,
                  "(1) the MM engine was not credited the row's MM half exactly once: an assumed shared pair must "
                  "seed BOTH engines");
        CLP_CHECK(r.goalOoT == 1 && r.goalMM == 1 && r.goalExpression == 1,
                  "(1) one assumed shared row must prove both halves");
        CLP_CHECK(ClContractClean(), "(1) no engine saw the other game's id");
    }

    // ---- (2) PLACED ON AN OoT HOST: OoT harvests its half, MM is handed its own --
    {
        ClBuildPairWorld(true, false);
        const ComboLogicBagItem bag[1] = { ClPairRow() };
        ComboLogicFillResult fr;
        const int rc =
            ClRunFill(bag, 1, (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x0976A001u, &fr);
        ComboLogicPlacement p;
        memset(&p, 0, sizeof(p));
        const bool placed = Combo_Logic_GetPlacement(GAME_OOT, 10, &p);
        printf("[TEST] (2) OoT host: fill rc %d, placed %d item %u/%u pair %u/%u, engine stored %u/%u\n", rc,
               placed ? 1 : 0, (unsigned)p.item.originGame, (unsigned)p.item.id, (unsigned)p.pair.originGame,
               (unsigned)p.pair.id, (unsigned)gClOoT.placedRaw[0].originGame, (unsigned)gClOoT.placedRaw[0].id);
        CLP_CHECK(rc == RSBS_COMBO_LOGIC_OK && fr.goalProven,
                  "(2) a shared row on an OoT host must prove beat-both: MM's half has to reach MM's engine");
        CLP_CHECK(placed && p.item.originGame == O && p.item.id == kOotPairMask,
                  "(2) the OoT host must hold OoT's OWN half of the pair");
        CLP_CHECK(p.pair.originGame == M && p.pair.id == kMmPairMask,
                  "(2) the placement must keep MM's half as `pair`");
        CLP_CHECK(gClOoT.placedCount == 1 && !gClOoT.placedIsCover[0] && gClOoT.placedRaw[0].originGame == O &&
                      gClOoT.placedRaw[0].id == kOotPairMask,
                  "(2) the OoT engine must be given its own id through place, never a cover");

        // One round over the placed world: each engine holds ONE copy.
        ComboLogicRoundResult r;
        memset(gClMM.assumeCalls, 0, sizeof(gClMM.assumeCalls));
        CLP_CHECK(ClRunRound((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &r) == RSBS_COMBO_LOGIC_OK,
                  "(2) the round over the placed world runs");
        printf("[TEST] (2) round: goalOoT %d goalMM %d exchanged %d, copies OoT %d MM %d (expected 1, 1, 1, 1, 1)\n",
               r.goalOoT, r.goalMM, r.exchanged, ClPairCopies(&gClOoT, kOotPairMask),
               ClPairCopies(&gClMM, kMmPairMask));
        CLP_CHECK(ClPairCopies(&gClOoT, kOotPairMask) == 1, "(2) OoT must harvest exactly one copy from its own host");
        CLP_CHECK(ClPairCopies(&gClMM, kMmPairMask) == 1 && r.exchanged == 1,
                  "(2) MM must be handed exactly one copy of its half, through the exchange");
        CLP_CHECK(r.goalOoT == 1 && r.goalMM == 1, "(2) both halves prove");

        // THE GATE: with OoT's crossing closed the player cannot carry the item
        // to Termina, so MM's engine must not be credited.
        gClOoT.crossingRequires = kClNeverBit;
        memset(gClMM.assumeCalls, 0, sizeof(gClMM.assumeCalls));
        CLP_CHECK(ClRunRound((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &r) == RSBS_COMBO_LOGIC_OK,
                  "(2) the gated round runs");
        printf("[TEST] (2) OoT crossing closed: MM copies %d, exchanged %d (expected 0, 0)\n",
               ClPairCopies(&gClMM, kMmPairMask), r.exchanged);
        CLP_CHECK(ClPairCopies(&gClMM, kMmPairMask) == 0 && r.exchanged == 0,
                  "(2) MM's half crossed while OoT's crossing was closed: the far half must wait for the gate");
        CLP_CHECK(ClPairCopies(&gClOoT, kOotPairMask) == 1, "(2) OoT's own half needs no gate");
        CLP_CHECK(ClContractClean(), "(2) no engine saw the other game's id");
    }

    // ---- (3) PLACED ON AN MM HOST: the far half needs BOTH crossings ---------
    {
        ClBuildPairWorld(false, true);
        const ComboLogicBagItem bag[1] = { ClPairRow() };
        ComboLogicFillResult fr;
        const int rc =
            ClRunFill(bag, 1, (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x0976A002u, &fr);
        ComboLogicPlacement p;
        memset(&p, 0, sizeof(p));
        const bool placed = Combo_Logic_GetPlacement(GAME_MM, 20, &p);
        printf("[TEST] (3) MM host: fill rc %d, placed %d item %u/%u pair %u/%u\n", rc, placed ? 1 : 0,
               (unsigned)p.item.originGame, (unsigned)p.item.id, (unsigned)p.pair.originGame, (unsigned)p.pair.id);
        CLP_CHECK(rc == RSBS_COMBO_LOGIC_OK && fr.goalProven,
                  "(3) a shared row on an MM host must prove beat-both: OoT's half has to reach OoT's engine");
        CLP_CHECK(placed && p.item.originGame == M && p.item.id == kMmPairMask && p.pair.originGame == O &&
                      p.pair.id == kOotPairMask,
                  "(3) the MM host must hold MM's OWN half, with OoT's half as `pair`");
        CLP_CHECK(gClMM.placedCount == 1 && !gClMM.placedIsCover[0] && gClMM.placedRaw[0].id == kMmPairMask,
                  "(3) the MM engine must be given its own id through place, never a cover");

        ComboLogicRoundResult r;
        gClMM.crossingRequires = kClNeverBit; // the player could reach Termina but never come back
        CLP_CHECK(ClRunRound((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, &r) == RSBS_COMBO_LOGIC_OK,
                  "(3) the gated round runs");
        printf("[TEST] (3) MM crossing closed: OoT copies %d, MM copies %d (expected 0, 1)\n",
               ClPairCopies(&gClOoT, kOotPairMask), ClPairCopies(&gClMM, kMmPairMask));
        CLP_CHECK(ClPairCopies(&gClOoT, kOotPairMask) == 0,
                  "(3) OoT's half crossed from an MM host with MM's crossing closed: it needs both crossings");
        CLP_CHECK(ClPairCopies(&gClMM, kMmPairMask) == 1, "(3) MM's own half needs no gate");
        CLP_CHECK(ClContractClean(), "(3) no engine saw the other game's id");
    }

    // ---- (4) THE ROW IS REFUSED WITH HOME_ONLY -------------------------------
    {
        ClBuildPairWorld(true, true);
        ComboLogicBagItem bag[1] = { ClPairRow() };
        bag[0].bagFlags |= RSBS_COMBO_BAG_HOME_ONLY;
        ComboLogicFillResult fr;
        const int rc =
            ClRunFill(bag, 1, (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x0976A003u, &fr);
        CLP_CHECK(rc == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST, "(4) a shared pair marked HOME_ONLY must be refused");
    }
    ClUninstall();

    // ---- (5) COMPOSE: both games' rows become ONE row ------------------------
    {
        const ClComposeSourceGuard guard;
        CLP_CHECK(guard.installed, "(5) both synthetic classification sources installed");
        // Synthetic ids 1 (progression in both games) form the family; 4 is junk.
        const ComboLogicPoolRow pool[] = { ClPoolRow(O, 1, 0), ClPoolRow(O, 4, 0), ClPoolRow(M, 1, 0) };
        ComboLogicComposeRequest req;
        memset(&req, 0, sizeof(req));
        req.rows = pool;
        req.rowCount = 3;
        req.startingHealthOoT = 0x30u;
        req.startingHealthMM = 0x30u;
        ComboLogicBagItem bag[8];
        int index[8];
        ComboLogicComposeResult res;

        CLP_CHECK(Combo_Logic_ComposeBag(&req, bag, 8, index, &res) == RSBS_COMBO_LOGIC_OK && res.bagCount == 2 &&
                      res.sharedPairs == 0,
                  "(5) control: with no family both games' rows are bag rows");

        const ComboLogicSharedPair fam = { 1, 1 };
        req.sharedPairs = &fam;
        req.sharedPairCount = 1;
        const int rc = Combo_Logic_ComposeBag(&req, bag, 8, index, &res);
        int trimmedAt = -1;
        const bool trimmed = Combo_Logic_ComposeTrimmedAt(0, &trimmedAt);
        printf("[TEST] (5) compose: rc %d bag %d pairs %d, row0 %u/%u flags %04X pairId %u, trimmed %d at %d\n", rc,
               res.bagCount, res.sharedPairs, (unsigned)bag[0].item.originGame, (unsigned)bag[0].item.id,
               (unsigned)bag[0].bagFlags, (unsigned)bag[0].pairId, Combo_Logic_ComposeTrimmedCount(), trimmedAt);
        CLP_CHECK(rc == RSBS_COMBO_LOGIC_OK && res.bagCount == 1 && res.sharedPairs == 1,
                  "(5) the family must leave exactly ONE bag row");
        CLP_CHECK(index[0] == 0 && bag[0].item.originGame == O && bag[0].item.id == 1 &&
                      bag[0].bagFlags == RSBS_COMBO_BAG_SHARED_PAIR && bag[0].pairId == 1,
                  "(5) the row at the EARLIER pool position must be the shared row, naming the other half's id");
        CLP_CHECK(Combo_Logic_ComposeTrimmedCount() == 1 && trimmed && trimmedAt == 2,
                  "(5) the other game's row must be TRIMMED to filler for its own pass");
        CLP_CHECK(res.perGame[M].rows[RSBS_COMBO_COMPOSE_TRIMMED] == 1 &&
                      res.perGame[M].rows[RSBS_COMBO_COMPOSE_REQUIRED] == 0 &&
                      res.perGame[O].rows[RSBS_COMBO_COMPOSE_REQUIRED] == 1,
                  "(5) the counts must move the dropped half from REQUIRED to TRIMMED under its own game");

        // MM's row first: the shared row is MM's, naming OoT's id.
        const ComboLogicPoolRow mmFirst[] = { ClPoolRow(M, 1, 0), ClPoolRow(O, 1, 0) };
        req.rows = mmFirst;
        req.rowCount = 2;
        CLP_CHECK(Combo_Logic_ComposeBag(&req, bag, 8, index, &res) == RSBS_COMBO_LOGIC_OK && res.bagCount == 1 &&
                      bag[0].item.originGame == M && bag[0].pairId == 1 && index[0] == 0,
                  "(5) with MM's row first the kept row is MM's, at its own position");

        // One side only: the row is kept and becomes the shared row; nothing is trimmed.
        const ComboLogicPoolRow oneSide[] = { ClPoolRow(O, 1, 0) };
        const ComboLogicSharedPair famOne = { 1, 77 };
        req.rows = oneSide;
        req.rowCount = 1;
        req.sharedPairs = &famOne;
        CLP_CHECK(Combo_Logic_ComposeBag(&req, bag, 8, index, &res) == RSBS_COMBO_LOGIC_OK && res.bagCount == 1 &&
                      bag[0].bagFlags == RSBS_COMBO_BAG_SHARED_PAIR && bag[0].pairId == 77 &&
                      Combo_Logic_ComposeTrimmedCount() == 0,
                  "(5) a family with one side's row keeps that copy as the shared row and trims nothing");

        req.sharedPairs = NULL;
        req.sharedPairCount = 1;
        CLP_CHECK(Combo_Logic_ComposeBag(&req, bag, 8, index, &res) == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(5) a positive family count with a NULL list must be refused");
    }

    printf("[TEST] combo-logic-shared-pair: %s\n", sClpFailures == 0 ? "PASS" : "FAIL");
    return sClpFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// combo-logic-fixed-credit — THE FIXED SHARED CREDIT (#1169 XW15)
// ============================================================================
//
// A shared family half that sits on its own game's check as FIXED native content
// (a shared warp song under Shuffle Songs Off) is in no table, so only a fixed
// credit hands the other game's half to the other engine. Each leg names the line
// of combo_logic.c's ComboLogicExchangeFrom credit step it would catch removed.

namespace {

const uint16_t kOotFixedSong = 106;
const uint16_t kMmFixedSong = 206;

/** OoT host 10 holds OoT's half as fixed content; MM host 20 holds MM's half only
 *  with @p mmFixed (a leg whose MM goal reads the credit must have no other MM
 *  source). Neither is a table row. Each host is open; MM's goal needs MM's half. */
void ClBuildFixedCreditWorld(bool mmFixed) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotFixedSong);
    ClAddItem(&gClOoT, kOotSword);
    ClAddHost(&gClOoT, 10, 0u);
    gClOoT.fixedItem[0] = kOotFixedSong;
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmFixedSong);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddHost(&gClMM, 20, 0u);
    gClMM.fixedItem[0] = mmFixed ? kMmFixedSong : (uint16_t)0;
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = ClBit(&gClMM, kMmFixedSong);
    ClInstall();
}

ComboLogicFixedCredit ClFixedCredit(uint8_t hostGame, uint16_t hostCheck, uint8_t farGame, uint16_t farId) {
    ComboLogicFixedCredit c;
    memset(&c, 0, sizeof(c));
    c.hostGame = hostGame;
    c.hostCheck = hostCheck;
    c.farItem = ClItem(farGame, farId);
    return c;
}

/** One beat-both round with every assume count zeroed first. */
int ClFixedCreditRound(ComboLogicRoundResult* r) {
    memset(gClOoT.assumeCalls, 0, sizeof(gClOoT.assumeCalls));
    memset(gClMM.assumeCalls, 0, sizeof(gClMM.assumeCalls));
    memset(r, 0, sizeof(*r));
    return ClRunRound((uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, NULL, 0, r);
}

/** Clears the credits and the stubs on every exit of the row, early ones too. */
struct ClFixedCreditGuard {
    ~ClFixedCreditGuard() {
        (void)Combo_Logic_SetFixedCredits(NULL, 0);
        ClUninstall();
    }
};

} // namespace

static int sClfFailures;
#define CLF_CHECK(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            sClfFailures++;                                                 \
        }                                                                   \
    } while (0)

TestResult Test_ComboLogicFixedCredit(void) {
    printf("[TEST] combo-logic-fixed-credit: a shared item fixed on its own game's check credits the other engine "
           "once per round when that check is reached, through the crossing gate, never from a forbidden host "
           "(#1169 XW15)\n");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    sClfFailures = 0;
    ClFixedCreditGuard guard;
    ClUninstall();
    (void)Combo_Logic_SetFixedCredits(NULL, 0);
    const ComboLogicFixedCredit ootHosted = ClFixedCredit(O, 10, M, kMmFixedSong);
    const ComboLogicFixedCredit mmHosted = ClFixedCredit(M, 20, O, kOotFixedSong);

    // ---- (1) THE HOST NOT REACHED: no grant (the checkReached term) ----------
    {
        ClBuildFixedCreditWorld(false);
        gClOoT.hostRequires[0] = ClBit(&gClOoT, kOotSword); // no source of the sword
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, 1) == 1, "(1) one credit is set");
        ComboLogicRoundResult r;
        const int rc = ClFixedCreditRound(&r);
        printf("[TEST] (1) host unreached: rc %d, MM assumes %d, delivered %d, goalMM %d (expected 0, 0, 0, 0)\n", rc,
               ClPairAssumeCalls(&gClMM, kMmFixedSong), Combo_Logic_FixedCreditDeliveredCount(), r.goalMM);
        CLF_CHECK(rc == RSBS_COMBO_LOGIC_OK, "(1) the round runs");
        CLF_CHECK(ClPairAssumeCalls(&gClMM, kMmFixedSong) == 0 && Combo_Logic_FixedCreditDeliveredCount() == 0,
                  "(1) the credit was delivered from a host the round never reached");
        CLF_CHECK(r.goalMM == 0, "(1) MM's half was held although its only source was never reached");
    }

    // ---- (2) REACHED, GATE OPEN: exactly one copy over every alternation -------
    {
        ClBuildFixedCreditWorld(false);
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, 1) == 1, "(2) one credit is set");
        ComboLogicRoundResult r;
        const int rc = ClFixedCreditRound(&r);
        printf("[TEST] (2) reached: rc %d, MM assumes %d, OoT assumes %d, OoT copies %d, delivered %d, iterations %d, "
               "goal %d/%d expr %d (expected 0, 1, 0, 1, 1, >=2, 1/1 1)\n",
               rc, ClPairAssumeCalls(&gClMM, kMmFixedSong), ClPairAssumeCalls(&gClOoT, kOotFixedSong),
               ClPairCopies(&gClOoT, kOotFixedSong), Combo_Logic_FixedCreditDeliveredCount(), r.iterations, r.goalOoT,
               r.goalMM, r.goalExpression);
        CLF_CHECK(rc == RSBS_COMBO_LOGIC_OK, "(2) the round runs (a credit re-delivered every alternation never "
                                             "converges)");
        CLF_CHECK(ClPairAssumeCalls(&gClMM, kMmFixedSong) == 1 && r.iterations >= 2,
                  "(2) MM's engine was not handed exactly ONE copy over a round of several alternations");
        CLF_CHECK(Combo_Logic_FixedCreditDeliveredCount() == 1, "(2) the delivered probe does not read one credit");
        CLF_CHECK(ClPairAssumeCalls(&gClOoT, kOotFixedSong) == 0 && ClPairCopies(&gClOoT, kOotFixedSong) == 1,
                  "(2) the host engine must harvest its own fixed half itself, never be assumed it");
        CLF_CHECK(r.goalMM == 1 && r.goalExpression == 1, "(2) the credit did not prove MM's half");
        CLF_CHECK(ClContractClean(), "(2) no engine saw the other game's id");
    }

    // ---- (3) THE GATE: OoT's crossing closed, nothing crosses from OoT ---------
    {
        ClBuildFixedCreditWorld(false);
        gClOoT.crossingRequires = kClNeverBit;
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, 1) == 1, "(3) one credit is set");
        ComboLogicRoundResult r;
        const int rc = ClFixedCreditRound(&r);
        printf("[TEST] (3) OoT crossing closed: rc %d, MM assumes %d, delivered %d (expected 0, 0, 0)\n", rc,
               ClPairAssumeCalls(&gClMM, kMmFixedSong), Combo_Logic_FixedCreditDeliveredCount());
        CLF_CHECK(rc == RSBS_COMBO_LOGIC_OK && ClPairAssumeCalls(&gClMM, kMmFixedSong) == 0 &&
                      Combo_Logic_FixedCreditDeliveredCount() == 0,
                  "(3) the credit crossed while OoT's crossing was closed: it must wait for the gate");

        // An MM host needs BOTH crossings: MM's closed, OoT's open. OoT's host
        // holds nothing here, so OoT's goal reads the credit alone.
        ClBuildFixedCreditWorld(true);
        gClOoT.fixedItem[0] = 0;
        gClOoT.goalRequires = ClBit(&gClOoT, kOotFixedSong);
        gClMM.goalRequires = 0u;
        gClMM.crossingRequires = kClNeverBit;
        CLF_CHECK(Combo_Logic_SetFixedCredits(&mmHosted, 1) == 1, "(3) the MM-hosted credit is set");
        const int rcClosed = ClFixedCreditRound(&r);
        const int closedAssumes = ClPairAssumeCalls(&gClOoT, kOotFixedSong);
        gClMM.crossingRequires = 0u;
        ClInstall();
        const int rcOpen = ClFixedCreditRound(&r);
        printf("[TEST] (3) MM host: MM crossing closed rc %d OoT assumes %d; open rc %d OoT assumes %d goalOoT %d "
               "(expected 0 0; 0 1 1)\n",
               rcClosed, closedAssumes, rcOpen, ClPairAssumeCalls(&gClOoT, kOotFixedSong), r.goalOoT);
        CLF_CHECK(rcClosed == RSBS_COMBO_LOGIC_OK && closedAssumes == 0,
                  "(3) OoT's half crossed from an MM host with MM's crossing closed: it needs both crossings");
        CLF_CHECK(rcOpen == RSBS_COMBO_LOGIC_OK && ClPairAssumeCalls(&gClOoT, kOotFixedSong) == 1 && r.goalOoT == 1,
                  "(3) with both crossings open the MM-hosted credit did not reach OoT's engine exactly once");
    }

    // ---- (4) A FORBIDDEN HOST: no credit (the sForbidden skip) ---------------
    {
        ClBuildFixedCreditWorld(false);
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, 1) == 1, "(4) one credit is set");
        memset(gClMM.assumeCalls, 0, sizeof(gClMM.assumeCalls));
        const ComboLogicHostRef forbid[1] = { ClHostRef(O, 10) };
        ComboLogicRoundResult r;
        const int rc = ClRunForbidden(forbid, 1, &r);
        printf("[TEST] (4) host forbidden: rc %d, MM assumes %d, delivered %d, goalMM %d (expected 0, 0, 0, 0)\n", rc,
               ClPairAssumeCalls(&gClMM, kMmFixedSong), Combo_Logic_FixedCreditDeliveredCount(), r.goalMM);
        CLF_CHECK(rc == RSBS_COMBO_LOGIC_OK && ClPairAssumeCalls(&gClMM, kMmFixedSong) == 0 &&
                      Combo_Logic_FixedCreditDeliveredCount() == 0 && r.goalMM == 0,
                  "(4) a forbidden host's fixed credit crossed: the round was asked about a world where it is empty");
    }

    // ---- (5) A CYCLE THROUGH THE CREDITS: the least fixpoint grants nothing ----
    {
        ClBuildFixedCreditWorld(true);
        gClOoT.hostRequires[0] = ClBit(&gClOoT, kOotFixedSong); // only from the MM-hosted credit
        gClMM.hostRequires[0] = ClBit(&gClMM, kMmFixedSong);    // only from the OoT-hosted credit
        gClOoT.goalRequires = ClBit(&gClOoT, kOotFixedSong);
        ClInstall();
        const ComboLogicFixedCredit both[2] = { ootHosted, mmHosted };
        CLF_CHECK(Combo_Logic_SetFixedCredits(both, 2) == 2, "(5) two credits are set");
        ComboLogicRoundResult r;
        const int rc = ClFixedCreditRound(&r);
        printf("[TEST] (5) cycle: rc %d, assumes OoT %d MM %d, delivered %d, expr %d (expected 0, 0 0, 0, 0)\n", rc,
               ClPairAssumeCalls(&gClOoT, kOotFixedSong), ClPairAssumeCalls(&gClMM, kMmFixedSong),
               Combo_Logic_FixedCreditDeliveredCount(), r.goalExpression);
        CLF_CHECK(rc == RSBS_COMBO_LOGIC_OK && Combo_Logic_FixedCreditDeliveredCount() == 0 &&
                      ClPairAssumeCalls(&gClOoT, kOotFixedSong) == 0 && ClPairAssumeCalls(&gClMM, kMmFixedSong) == 0 &&
                      r.goalExpression == 0,
                  "(5) a host reachable only through its own credit was credited: a credit must never be assumed");
        // Control: open OoT's host and both credits flow, each once.
        gClOoT.hostRequires[0] = 0u;
        ClInstall();
        const int rcOpen = ClFixedCreditRound(&r);
        printf("[TEST] (5) cycle broken: rc %d, delivered %d, expr %d (expected 0, 2, 1)\n", rcOpen,
               Combo_Logic_FixedCreditDeliveredCount(), r.goalExpression);
        CLF_CHECK(rcOpen == RSBS_COMBO_LOGIC_OK && Combo_Logic_FixedCreditDeliveredCount() == 2 &&
                      r.goalExpression == 1,
                  "(5) with OoT's host open both credits must be delivered and the goal proven");
    }

    // ---- (6) CLEARED: nothing is credited -------------------------------------
    {
        ClBuildFixedCreditWorld(false);
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, 1) == 1, "(6) one credit is set");
        CLF_CHECK(Combo_Logic_SetFixedCredits(NULL, 0) == 0 && Combo_Logic_FixedCreditCount() == 0,
                  "(6) NULL with 0 must clear the credits");
        ComboLogicRoundResult r;
        const int rc = ClFixedCreditRound(&r);
        CLF_CHECK(rc == RSBS_COMBO_LOGIC_OK && ClPairAssumeCalls(&gClMM, kMmFixedSong) == 0 && r.goalMM == 0,
                  "(6) a cleared credit still crossed");
    }

    // ---- (7) REFUSALS: nothing changes ------------------------------------------
    {
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, 1) == 1, "(7) one credit is set");
        ComboLogicFixedCredit many[RSBS_COMBO_LOGIC_FIXED_CREDIT_CAP + 1];
        for (int i = 0; i < RSBS_COMBO_LOGIC_FIXED_CREDIT_CAP + 1; ++i) {
            many[i] = ootHosted;
        }
        CLF_CHECK(Combo_Logic_SetFixedCredits(many, RSBS_COMBO_LOGIC_FIXED_CREDIT_CAP + 1) == -1,
                  "(7) more credits than the cap must be refused");
        CLF_CHECK(Combo_Logic_SetFixedCredits(NULL, 1) == -1, "(7) NULL with a positive count must be refused");
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, -1) == -1, "(7) a negative count must be refused");
        const ComboLogicFixedCredit noGame = ClFixedCredit((uint8_t)GAME_NONE, 10, M, kMmFixedSong);
        CLF_CHECK(Combo_Logic_SetFixedCredits(&noGame, 1) == -1, "(7) a host game that is no game must be refused");
        const ComboLogicFixedCredit sameGame = ClFixedCredit(O, 10, O, kOotFixedSong);
        CLF_CHECK(Combo_Logic_SetFixedCredits(&sameGame, 1) == -1,
                  "(7) a far item of the host's own game must be refused");
        ComboLogicFixedCredit got;
        memset(&got, 0, sizeof(got));
        CLF_CHECK(Combo_Logic_FixedCreditCount() == 1 && Combo_Logic_FixedCreditAt(0, &got) && got.hostGame == O &&
                      got.hostCheck == 10 && got.farItem.originGame == M && got.farItem.id == kMmFixedSong,
                  "(7) a refused call changed the credits in force");
        CLF_CHECK(!Combo_Logic_FixedCreditAt(1, &got) && !Combo_Logic_FixedCreditAt(-1, &got) &&
                      !Combo_Logic_FixedCreditAt(0, NULL),
                  "(7) an out-of-range or NULL read must answer false");
    }

    // ---- (8) AN INPUT OF THE WORLD: kept by the reset, cleared by the forget -----
    {
        ClBuildFixedCreditWorld(false);
        CLF_CHECK(Combo_Logic_SetFixedCredits(&ootHosted, 1) == 1, "(8) one credit is set");
        Combo_Logic_ResetPlacements();
        ComboLogicRoundResult r;
        const int rc = ClFixedCreditRound(&r);
        printf("[TEST] (8) after the reset: count %d, rc %d, delivered %d (expected 1, 0, 1)\n",
               Combo_Logic_FixedCreditCount(), rc, Combo_Logic_FixedCreditDeliveredCount());
        CLF_CHECK(Combo_Logic_FixedCreditCount() == 1 && rc == RSBS_COMBO_LOGIC_OK &&
                      Combo_Logic_FixedCreditDeliveredCount() == 1,
                  "(8) Combo_Logic_ResetPlacements dropped a credit: a fill retry would lose the other game's song");
        Combo_SingleBag_Forget();
        CLF_CHECK(Combo_Logic_FixedCreditCount() == 0,
                  "(8) Combo_SingleBag_Forget left the credits in force for the next world");
    }

    printf("[TEST] combo-logic-fixed-credit: %s\n", sClfFailures == 0 ? "PASS" : "FAIL");
    return sClfFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// combo-logic-host-groups — THE HOST GROUPS (ABI 9, #1502)
// ============================================================================
//
// A confined row (ComboLogicFillRequest.confineMasks) may only be drawn onto a host
// of its own game whose `hostGroups` answer meets its mask: MM's Own Dungeon and
// Song Locations. Each leg names the rule it would catch removed.

namespace {

/** MM host 22 is the one host of group bit 0 unless a leg says otherwise. */
const uint32_t kClGroupA = 0x1u;
const uint32_t kClGroupB = 0x2u; // a group no authored host is in

/** A world for the group legs: OoT hosts 10 and 11 (11 in group A of OoT's own
 *  namespace, to catch a predicate that forgot the origin), MM hosts 20 ..
 *  20 + mmHosts - 1, all sphere zero except `mmHostNeedsOcarina` (0 for none);
 *  MM host 22 in group A, and also `alsoInA` (an MM host id, 0 for none). Every
 *  bag id the legs use is in its engine's table, and both goals are
 *  unconditional. */
void ClBuildGroupWorld(int mmHosts, uint16_t alsoInA, uint16_t mmHostNeedsOcarina) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotPairMask);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, 0u);
    gClOoT.withHostGroups = true;
    gClOoT.hostGroup[1] = kClGroupA;
    gClOoT.crossingRequires = 0u;
    gClOoT.goalRequires = 0u;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddItem(&gClMM, kMmMask);
    ClAddItem(&gClMM, kMmPairMask);
    for (int i = 0; i < mmHosts; ++i) {
        const uint16_t id = (uint16_t)(20 + i);
        ClAddHost(&gClMM, id, (id == mmHostNeedsOcarina) ? ClBit(&gClMM, kMmOcarina) : 0u);
        if (id == 22 || (alsoInA != 0 && id == alsoInA)) {
            gClMM.hostGroup[i] = kClGroupA;
        }
    }
    gClMM.withHostGroups = true;
    gClMM.crossingRequires = 0u;
    gClMM.goalRequires = 0u;
    ClInstall();
}

int ClRunConfinedFill(const ComboLogicBagItem* bag, int bagCount, const uint32_t* masks, uint8_t rung, uint32_t seed,
                      ComboLogicFillResult* out) {
    ComboLogicFillRequest req;
    memset(&req, 0, sizeof(req));
    req.bag = bag;
    req.bagCount = bagCount;
    req.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH;
    req.logicRung = rung;
    req.seed = seed;
    req.confineMasks = masks;
    return Combo_Logic_RunFill(&req, out);
}

/** Which host holds MM item `id`, in either game's table? -1 when none does. */
int ClHostOfMM(uint16_t id, GameId* outGame) {
    const GameId both[2] = { GAME_OOT, GAME_MM };
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < Combo_Logic_PlacementCount(both[s]); ++i) {
            ComboLogicPlacement p;
            if (Combo_Logic_PlacementAt(both[s], i, &p) && p.item.originGame == (uint8_t)GAME_MM && p.item.id == id) {
                if (outGame != NULL) {
                    *outGame = both[s];
                }
                return (int)p.hostCheck;
            }
        }
    }
    return -1;
}

ComboLogicBagItem ClHomeItem(uint8_t origin, uint16_t id) {
    ComboLogicBagItem b = ClBagItem(origin, id, RSBS_ITEMCLASS_PROGRESSION);
    b.bagFlags = RSBS_COMBO_BAG_HOME_ONLY;
    return b;
}

} // namespace

// A SOFT check, like CLP_CHECK: every leg runs after one fails.
static int sClgFailures;
#define CLG_CHECK(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            sClgFailures++;                                                 \
        }                                                                   \
    } while (0)

TestResult Test_ComboLogicHostGroups(void) {
    printf("[TEST] combo-logic-host-groups: a confined row lands only on its own game's hosts in its group, is drawn "
           "first, drops alone as surplus, and a group with more rows than hosts is refused up front (ABI 9, #1502)\n");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    sClgFailures = 0;
    ClUninstall();
    const int kSeeds = 32;

    // ---- (1) THE PREDICATE, under both host sources ----------------------------
    // One MM row confined to group A, whose only MM host is 22 (OoT host 11 is in
    // group A of OoT's namespace, which must not count). Catches: the group test
    // missing from ComboLogicRowMayUseHost, the whole-side shortcut not refused for
    // a confined row (the row would land anywhere on MM's side), and a predicate
    // that lets the row cross onto the other game's group hosts (OoT host 11).
    {
        const uint8_t rungs[2] = { (uint8_t)RSBS_COMBO_RUNG_NONE, (uint8_t)RSBS_COMBO_RUNG_BEATABLE };
        for (int r = 0; r < 2; ++r) {
            int offGroup = 0;
            int failed = 0;
            int ootAsked = 0;
            int mmAsked = 0;
            for (int s = 1; s <= kSeeds; ++s) {
                ClBuildGroupWorld(6, 0, 0);
                const ComboLogicBagItem bag[1] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION) };
                const uint32_t masks[1] = { kClGroupA };
                ComboLogicFillResult fr;
                const int rc = ClRunConfinedFill(bag, 1, masks, rungs[r], 0x15020000u + (uint32_t)s, &fr);
                GameId g = GAME_NONE;
                const int host = ClHostOfMM(kMmBow, &g);
                failed += (rc != RSBS_COMBO_LOGIC_OK) ? 1 : 0;
                offGroup += (host != 22 || g != GAME_MM) ? 1 : 0;
                ootAsked += gClOoT.hostGroupCalls;
                mmAsked += gClMM.hostGroupCalls;
            }
            printf("[TEST] (1) rung %u: %d of %d seeds failed, %d placed off group A, group queries OoT %d MM %d "
                   "(expected 0, 0, 0, >0)\n",
                   (unsigned)rungs[r], failed, kSeeds, offGroup, ootAsked, mmAsked);
            CLG_CHECK(failed == 0, "(1) a confined row with a free group host must fill");
            CLG_CHECK(offGroup == 0, "(1) a confined row landed off its group: the predicate or the whole-side "
                                     "refusal is missing, or the row was let onto the other game's hosts");
            CLG_CHECK(ootAsked == 0 && mmAsked > 0, "(1) only the row's own engine may be asked for host groups");
        }

        // THE CONTROL: the same row unconfined lands elsewhere on some seed (the
        // world has seven other hosts), and no engine is ever asked for a group.
        int elsewhere = 0;
        int asked = 0;
        for (int s = 1; s <= kSeeds; ++s) {
            ClBuildGroupWorld(6, 0, 0);
            const ComboLogicBagItem bag[1] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION) };
            ComboLogicFillResult fr;
            (void)ClRunConfinedFill(bag, 1, NULL, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15020000u + (uint32_t)s, &fr);
            elsewhere += (ClHostOfMM(kMmBow, NULL) != 22) ? 1 : 0;
            asked += gClMM.hostGroupCalls + gClOoT.hostGroupCalls;
        }
        printf("[TEST] (1) control: unconfined, %d of %d seeds off host 22, %d group queries (expected >0, 0)\n",
               elsewhere, kSeeds, asked);
        CLG_CHECK(elsewhere > 0, "(1) control: the unconfined row never left host 22, so leg 1 proves nothing");
        CLG_CHECK(asked == 0, "(1) control: a fill with no masks must never ask an engine for host groups");
    }

    // ---- (2) NO MASK IS THE ABI-8 FILL ------------------------------------------
    // An all-zero mask array must fill byte-identically to NULL and ask no engine
    // for a group. Catches any mask path (the partition, the capacity check's
    // reset, the whole-side refusal) that runs for a mask-free bag and would move
    // every default world.
    {
        const ComboLogicBagItem bag[4] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(M, kMmRemains, RSBS_ITEMCLASS_PROGRESSION),
                                           ClSurplusItem(M, kMmMask, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t zeros[4] = { 0u, 0u, 0u, 0u };
        int differ = 0;
        for (int s = 1; s <= kSeeds; ++s) {
            ClBuildGroupWorld(6, 0, 0);
            ComboLogicFillResult a;
            ComboLogicFillResult b;
            const int ra =
                ClRunConfinedFill(bag, 4, NULL, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x15021000u + (uint32_t)s, &a);
            ClBuildGroupWorld(6, 0, 0);
            const int rb =
                ClRunConfinedFill(bag, 4, zeros, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x15021000u + (uint32_t)s, &b);
            differ += (ra != rb || a.placementDigest != b.placementDigest || a.droppedDigest != b.droppedDigest ||
                       a.rounds != b.rounds || gClMM.hostGroupCalls != 0)
                          ? 1
                          : 0;
        }
        printf("[TEST] (2) all-zero masks vs none: %d of %d seeds differ (expected 0)\n", differ, kSeeds);
        CLG_CHECK(differ == 0, "(2) an all-zero mask array changed the fill: default worlds would move");
    }

    // ---- (3) CONFINED ROWS FIRST ------------------------------------------------
    // MM has exactly four hosts, one of them (22) in group A; the bag is three
    // home-only MM rows and, LAST in bag order, one row confined to A. A general
    // row drawn before it takes host 22 on some seed unless confined rows go first,
    // and the confined row then has nowhere to go. Catches the partition removed
    // from either fill path (this leg runs `none` and `beatable`).
    {
        const ComboLogicBagItem bag[4] = { ClHomeItem(M, kMmOcarina), ClHomeItem(M, kMmRemains), ClHomeItem(M, kMmMask),
                                           ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t masks[4] = { 0u, 0u, 0u, kClGroupA };
        const uint8_t rungs[2] = { (uint8_t)RSBS_COMBO_RUNG_NONE, (uint8_t)RSBS_COMBO_RUNG_BEATABLE };
        for (int r = 0; r < 2; ++r) {
            int bad = 0;
            for (int s = 1; s <= kSeeds; ++s) {
                ClBuildGroupWorld(4, 0, 0);
                ComboLogicFillResult fr;
                const int rc = ClRunConfinedFill(bag, 4, masks, rungs[r], 0x15022000u + (uint32_t)s, &fr);
                bad += (rc != RSBS_COMBO_LOGIC_OK || ClHostOfMM(kMmBow, NULL) != 22 || fr.attempts != 1) ? 1 : 0;
            }
            printf("[TEST] (3) rung %u: %d of %d seeds did not place the confined row on 22 in one attempt "
                   "(expected 0)\n",
                   (unsigned)rungs[r], bad, kSeeds);
            CLG_CHECK(bad == 0, "(3) a general row took the group's only host: confined rows are not drawn first");
        }
    }

    // ---- (4) SOUNDNESS: never on a group host that needs the row itself ---------
    // Group A is hosts 22 (sphere zero) and 23 (needs the Ocarina); the confined
    // row IS the Ocarina. A proving rung draws from hosts reached with the row
    // itself NOT assumed, so 23 is never offered. Catches a confined pass that
    // draws from every empty host (2Ship's PreplaceConfinedItems) under a proving
    // rung, which can lock a key behind its own door.
    {
        int bad = 0;
        for (int s = 1; s <= kSeeds; ++s) {
            ClBuildGroupWorld(6, 23, 23);
            const ComboLogicBagItem bag[3] = { ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION),
                                               ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                               ClBagItem(M, kMmRemains, RSBS_ITEMCLASS_PROGRESSION) };
            const uint32_t masks[3] = { kClGroupA, 0u, 0u };
            ComboLogicFillResult fr;
            const int rc =
                ClRunConfinedFill(bag, 3, masks, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x15023000u + (uint32_t)s, &fr);
            bad += (rc != RSBS_COMBO_LOGIC_OK || ClHostOfMM(kMmOcarina, NULL) != 22) ? 1 : 0;
        }
        printf("[TEST] (4) beatable: %d of %d seeds put the Ocarina anywhere but 22 (expected 0)\n", bad, kSeeds);
        CLG_CHECK(bad == 0, "(4) a confined row was placed behind itself (or the fill failed)");
        CLG_CHECK(ClContractClean(), "(4) no engine saw the other game's id");
    }

    // ---- (5) THE CAPACITY REFUSAL -----------------------------------------------
    // Two REQUIRED rows confined to A, one group host. Catches: the check missing
    // (the fill would run and dead-end), running after an attempt (attempts != 0),
    // a shortfall naming the wrong group, and the status read as a world dead end
    // the ladder may climb (no seed has more checks).
    {
        const ComboLogicBagItem bag[3] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(M, kMmRemains, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(M, kMmMask, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t masks[3] = { kClGroupA, 0u, kClGroupA };
        const uint8_t rungs[3] = { (uint8_t)RSBS_COMBO_RUNG_NONE, (uint8_t)RSBS_COMBO_RUNG_BEATABLE,
                                   (uint8_t)RSBS_COMBO_RUNG_ALL_REACHABLE };
        for (int r = 0; r < 3; ++r) {
            ClBuildGroupWorld(6, 0, 0);
            ComboLogicFillResult fr;
            const int rc = ClRunConfinedFill(bag, 3, masks, rungs[r], 0x15024000u, &fr);
            ComboLogicGroupShortfall sf;
            memset(&sf, 0, sizeof(sf));
            const bool named = Combo_Logic_LastGroupShortfall(&sf);
            printf("[TEST] (5) rung %u: rc %s attempts %d rounds %d placed %d, shortfall %d game %u mask %08X rows "
                   "%d hosts %d (expected group-capacity, 0, 0, 0, 1, MM, 1, 2, 1)\n",
                   (unsigned)rungs[r], Combo_Logic_StatusName(rc), fr.attempts, fr.rounds, fr.placed, named ? 1 : 0,
                   (unsigned)sf.game, (unsigned)sf.mask, sf.rows, sf.hosts);
            CLG_CHECK(rc == RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY, "(5) two rows for one group host must be refused");
            CLG_CHECK(fr.attempts == 0 && fr.rounds == 0 && fr.placed == 0,
                      "(5) the refusal must come before any attempt or round");
            CLG_CHECK(named && sf.game == M && sf.mask == kClGroupA && sf.rows == 2 && sf.hosts == 1,
                      "(5) the shortfall must name MM, group A, 2 rows, 1 host");
            CLG_CHECK(!Combo_SingleBag_StatusIsWorldDeadEnd(RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY, rungs[r]),
                      "(5) a group shortfall is a fact of the settings, never a ladder rung");
        }
        CLG_CHECK(strcmp(Combo_Logic_StatusName(RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY), "group-capacity") == 0,
                  "(5) the status has its own name");

        // A second group host fits; the shortfall is forgotten by the next fill.
        ClBuildGroupWorld(6, 24, 0);
        ComboLogicFillResult fr;
        const int rc = ClRunConfinedFill(bag, 3, masks, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15024001u, &fr);
        const int a = ClHostOfMM(kMmBow, NULL);
        const int b = ClHostOfMM(kMmMask, NULL);
        CLG_CHECK(rc == RSBS_COMBO_LOGIC_OK && !Combo_Logic_LastGroupShortfall(NULL),
                  "(5) with two group hosts the same bag fills, and no shortfall is left behind");
        CLG_CHECK((a == 22 && b == 24) || (a == 24 && b == 22), "(5) both confined rows take the two group hosts");

        // A SURPLUS row is not counted: one required and one surplus row, one host.
        ClBuildGroupWorld(6, 0, 0);
        const ComboLogicBagItem withSurplus[2] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                                   ClSurplusItem(M, kMmMask, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t both[2] = { kClGroupA, kClGroupA };
        const int rcSurplus = ClRunConfinedFill(withSurplus, 2, both, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15024002u, &fr);
        CLG_CHECK(rcSurplus == RSBS_COMBO_LOGIC_OK && fr.surplusDropped == 1,
                  "(5) a surplus copy is droppable, so it never makes a group refuse");
    }

    // ---- (6) A CONFINED SURPLUS ROW DROPS ALONE ---------------------------------
    // Surplus rows in bag order: S confined to group B (no host is in it), then T
    // free. S dead-ends on its group, not on the supply, so T must still be
    // placed. Catches the drop-alone rule removed (the tail rule drops T with S).
    {
        ClBuildGroupWorld(6, 0, 0);
        const ComboLogicBagItem bag[3] = { ClHomeItem(M, kMmBow), ClSurplusItem(M, kMmMask, RSBS_ITEMCLASS_PROGRESSION),
                                           ClSurplusItem(M, kMmRemains, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t masks[3] = { 0u, kClGroupB, 0u };
        ComboLogicFillResult fr;
        const int rc = ClRunConfinedFill(bag, 3, masks, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x15025000u, &fr);
        printf("[TEST] (6) rc %s surplus placed %d dropped %d, free surplus on %d (expected ok, 1, 1, a host)\n",
               Combo_Logic_StatusName(rc), fr.surplusPlaced, fr.surplusDropped, ClHostOfMM(kMmRemains, NULL));
        CLG_CHECK(rc == RSBS_COMBO_LOGIC_OK && fr.surplusPlaced == 1 && fr.surplusDropped == 1 &&
                      ClHostOfMM(kMmRemains, NULL) >= 0 && ClHostOfMM(kMmMask, NULL) < 0,
                  "(6) a confined surplus row with no group host must drop ALONE, the free one after it placed");
    }

    // ---- (7) REFUSALS AND THE FORCED ROWS ---------------------------------------
    // Catches: a shared pair with a LOCAL mask accepted (it has no single home; a
    // wide-only pair is legal since ABI 11, combo-logic-song-pool (7)), a confined
    // row whose engine cannot answer groups dead-ending instead of refusing, and a
    // forced debug row bypassing the predicate.
    {
        ClBuildGroupWorld(6, 0, 0);
        const ComboLogicBagItem pair[1] = { ClPairRow() };
        const uint32_t one[1] = { kClGroupA };
        ComboLogicFillResult fr;
        CLG_CHECK(ClRunConfinedFill(pair, 1, one, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15026000u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(7) a shared pair with a local confinement mask must be refused");

        ClBuildGroupWorld(6, 0, 0);
        gClMM.withHostGroups = false;
        ClInstall();
        const ComboLogicBagItem row[1] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION) };
        CLG_CHECK(ClRunConfinedFill(row, 1, one, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15026001u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(7) a confined row whose engine has no hostGroups must be refused");
        const uint32_t none[1] = { 0u };
        CLG_CHECK(ClRunConfinedFill(row, 1, none, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15026002u, &fr) ==
                      RSBS_COMBO_LOGIC_OK,
                  "(7) control: an unconfined row needs no hostGroups");

        ClBuildGroupWorld(6, 0, 0);
        ComboLogicForcedPlacement forced;
        memset(&forced, 0, sizeof(forced));
        forced.hostGame = M;
        forced.hostCheck = 21; // not in group A
        forced.item = ClItem(M, kMmBow);
        CLG_CHECK(Combo_Logic_TestSetForcedPlacements(&forced, 1) == 1, "(7) the forced row is armed");
        const int offGroup = ClRunConfinedFill(row, 1, one, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15026003u, &fr);
        (void)Combo_Logic_TestSetForcedPlacements(NULL, 0);
        CLG_CHECK(offGroup == RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(7) a forced confined row must be refused off its group");

        // On its group: two hosts in group A (22 and 24), the row forced onto 24.
        // Over the seed loop an ordinary draw lands on 22 at least once (the
        // control, unforced), so "on 24 every seed" proves the forced row was
        // placed, not merely drawn into the group (red if the forced claim skips a
        // confined row).
        int inGroupOk = 0;
        int onForced = 0;
        int controlOn22 = 0;
        const int kForcedSeeds = 16;
        for (int s = 0; s < kForcedSeeds; ++s) {
            const uint32_t seed = 0x15026100u + (uint32_t)s;
            (void)Combo_Logic_TestSetForcedPlacements(NULL, 0);
            ClBuildGroupWorld(6, 24, 0);
            if (ClRunConfinedFill(row, 1, one, (uint8_t)RSBS_COMBO_RUNG_NONE, seed, &fr) == RSBS_COMBO_LOGIC_OK &&
                ClHostOfMM(kMmBow, NULL) == 22) {
                controlOn22++;
            }
            forced.hostCheck = 24;
            CLG_CHECK(Combo_Logic_TestSetForcedPlacements(&forced, 1) == 1, "(7) the forced row is re-armed");
            ClBuildGroupWorld(6, 24, 0);
            if (ClRunConfinedFill(row, 1, one, (uint8_t)RSBS_COMBO_RUNG_NONE, seed, &fr) == RSBS_COMBO_LOGIC_OK) {
                inGroupOk++;
                onForced += (ClHostOfMM(kMmBow, NULL) == 24) ? 1 : 0;
            }
        }
        (void)Combo_Logic_TestSetForcedPlacements(NULL, 0);
        printf("[TEST] (7) forced onto group host 24: %d/%d accepted, %d on 24; unforced control on 22 %d time(s)\n",
               inGroupOk, kForcedSeeds, onForced, controlOn22);
        CLG_CHECK(controlOn22 > 0, "(7) control: the unforced draw never chose 22, so the forced leg proves nothing");
        CLG_CHECK(inGroupOk == kForcedSeeds && onForced == kForcedSeeds,
                  "(7) a forced confined row on its group must be accepted and placed on its forced host");
    }

    ClUninstall();
    printf("[TEST] combo-logic-host-groups: %s\n", sClgFailures == 0 ? "PASS" : "FAIL");
    return sClgFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// combo-logic-song-pool — THE COMBO-WIDE GROUPS (ABI 11, #1542)
// ============================================================================
//
// A row whose mask is a combo-wide group (RSBS_COMBO_GROUP_WIDE_MASK) may land on
// the hosts of EITHER game in that group, the other game's only where that host
// accepts foreign items and the row is not HOME_ONLY; the request's exclusive
// groups keep every unconfined row off those hosts (OoTMM songLocations); capacity
// is Hall's condition over the row and host classes. Each leg names the rule it
// would catch removed.

namespace {

const uint32_t kClWide = RSBS_COMBO_GROUP_SONG_LOCATIONS;
const uint32_t kClWide2 = 0x02000000u; // a wide group no authored host is in

/** OoT hosts 10, 11 (wide "song checks") and 12, 13, 14; MM hosts 20 (wide) and
 *  21, 22, 23; both engines declare `declared` (0: no wideGroups entry). Sphere
 *  zero everywhere, both goals unconditional, every id the legs use in its table. */
void ClBuildSongWorld(uint32_t declared) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddItem(&gClOoT, kOotBoots);
    ClAddItem(&gClOoT, kOotPairMask);
    for (uint16_t id = 10; id <= 14; ++id) {
        ClAddHost(&gClOoT, id, 0u);
    }
    gClOoT.hostGroup[0] = kClWide;
    gClOoT.hostGroup[1] = kClWide;
    gClOoT.withHostGroups = true;
    gClOoT.withWideGroups = declared != 0u;
    gClOoT.wideDeclared = declared;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddItem(&gClMM, kMmMask);
    ClAddItem(&gClMM, kMmPairMask);
    for (uint16_t id = 20; id <= 23; ++id) {
        ClAddHost(&gClMM, id, 0u);
    }
    gClMM.hostGroup[0] = kClWide;
    gClMM.withHostGroups = true;
    gClMM.withWideGroups = declared != 0u;
    gClMM.wideDeclared = declared;
    ClInstall();
}

int ClRunSongFill(const ComboLogicBagItem* bag, int bagCount, const uint32_t* masks, uint32_t exclusive, uint8_t rung,
                  uint32_t seed, ComboLogicFillResult* out) {
    ComboLogicFillRequest req;
    memset(&req, 0, sizeof(req));
    req.bag = bag;
    req.bagCount = bagCount;
    req.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH;
    req.logicRung = rung;
    req.seed = seed;
    req.confineMasks = masks;
    req.exclusiveGroups = exclusive;
    return Combo_Logic_RunFill(&req, out);
}

/** Which host holds item (`origin`, `id`), in either game's table? -1 when none
 *  does; the host's game in `outGame`. */
int ClHostOfItem(uint8_t origin, uint16_t id, GameId* outGame) {
    const GameId both[2] = { GAME_OOT, GAME_MM };
    for (int g = 0; g < 2; ++g) {
        for (int i = 0; i < Combo_Logic_PlacementCount(both[g]); ++i) {
            ComboLogicPlacement p;
            if (Combo_Logic_PlacementAt(both[g], i, &p) && p.item.originGame == origin && p.item.id == id) {
                if (outGame != NULL) {
                    *outGame = both[g];
                }
                return (int)p.hostCheck;
            }
        }
    }
    return -1;
}

bool ClIsSongHost(GameId game, int host) {
    return (game == GAME_OOT && (host == 10 || host == 11)) || (game == GAME_MM && host == 20);
}

// Leg (11)'s world: OoT hosts 40.. and MM hosts 60.., ascending, the first
// `ootWide` / `mmWide` of each in the wide group and two plain hosts after them.
const uint16_t kClStarveOoTBase = 40;
const uint16_t kClStarveMMBase = 60;
const int kClStarvePlain = 2;

void ClBuildStarveWorld(int ootWide, int mmWide) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    for (int i = 0; i < ootWide + kClStarvePlain; ++i) {
        ClAddHost(&gClOoT, (uint16_t)(kClStarveOoTBase + i), 0u);
        gClOoT.hostGroup[i] = (i < ootWide) ? kClWide : 0u;
    }
    gClOoT.withHostGroups = true;
    gClOoT.withWideGroups = true;
    gClOoT.wideDeclared = kClWide;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    for (int i = 0; i < mmWide + kClStarvePlain; ++i) {
        ClAddHost(&gClMM, (uint16_t)(kClStarveMMBase + i), 0u);
        gClMM.hostGroup[i] = (i < mmWide) ? kClWide : 0u;
    }
    gClMM.withHostGroups = true;
    gClMM.withWideGroups = true;
    gClMM.wideDeclared = kClWide;
    ClInstall();
}

/** Is `host` of `game` one of leg (11)'s wide hosts? */
bool ClIsStarveWideHost(GameId game, uint16_t host, int ootWide, int mmWide) {
    const ClEngine* e = (game == GAME_OOT) ? &gClOoT : &gClMM;
    const int i = ClHostIndex(e, host);
    return i >= 0 && i < ((game == GAME_OOT) ? ootWide : mmWide);
}

} // namespace

static int sClsFailures;
#define CLS_CHECK(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            sClsFailures++;                                                 \
        }                                                                   \
    } while (0)

TestResult Test_ComboLogicSongPool(void) {
    printf("[TEST] combo-logic-song-pool: a combo-wide row lands only on the wide hosts of either game, crosses only "
           "where the host accepts foreign items, an exclusive group keeps every other row off them, and capacity is "
           "Hall's condition over both games (ABI 11, #1542)\n");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    sClsFailures = 0;
    ClUninstall();
    const int kSeeds = 32;

    // ---- (1) THE PREDICATE, both origins, both host sources ---------------------
    // Catches: the wide test missing (a song on a plain host), the whole-side
    // shortcut left on for a wide row or for a side holding an exclusive host (an
    // unconfined row on a song host), and a wide row barred from crossing (never on
    // the other game's song host).
    {
        const uint8_t rungs[2] = { (uint8_t)RSBS_COMBO_RUNG_NONE, (uint8_t)RSBS_COMBO_RUNG_BEATABLE };
        for (int r = 0; r < 2; ++r) {
            int offSong = 0;
            int unconfinedOnSong = 0;
            int failed = 0;
            int ootSongInMM = 0;
            int mmSongInOoT = 0;
            for (int sd = 1; sd <= kSeeds; ++sd) {
                ClBuildSongWorld(kClWide);
                const ComboLogicBagItem bag[4] = { ClBagItem(O, kOotHook, RSBS_ITEMCLASS_PROGRESSION),
                                                   ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                                   ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                                   ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION) };
                const uint32_t masks[4] = { kClWide, kClWide, 0u, 0u };
                ComboLogicFillResult fr;
                const int rc = ClRunSongFill(bag, 4, masks, kClWide, rungs[r], 0x15420000u + (uint32_t)sd, &fr);
                if (rc != RSBS_COMBO_LOGIC_OK) {
                    failed++;
                    continue;
                }
                GameId g = GAME_NONE;
                int h = ClHostOfItem(O, kOotHook, &g);
                offSong += ClIsSongHost(g, h) ? 0 : 1;
                ootSongInMM += (g == GAME_MM) ? 1 : 0;
                h = ClHostOfItem(M, kMmBow, &g);
                offSong += ClIsSongHost(g, h) ? 0 : 1;
                mmSongInOoT += (g == GAME_OOT) ? 1 : 0;
                h = ClHostOfItem(O, kOotSword, &g);
                unconfinedOnSong += ClIsSongHost(g, h) ? 1 : 0;
                h = ClHostOfItem(M, kMmOcarina, &g);
                unconfinedOnSong += ClIsSongHost(g, h) ? 1 : 0;
            }
            printf("[TEST] (1) rung %u: %d failed, %d wide rows off the song hosts, %d unconfined rows on them, OoT "
                   "song in MM %d, MM song in OoT %d\n",
                   (unsigned)rungs[r], failed, offSong, unconfinedOnSong, ootSongInMM, mmSongInOoT);
            CLS_CHECK(failed == 0, "(1) a fill with room for every row failed");
            CLS_CHECK(offSong == 0, "(1) a wide row landed off the song hosts of both games");
            CLS_CHECK(unconfinedOnSong == 0, "(1) an unconfined row took an exclusive song host");
            CLS_CHECK(ootSongInMM > 0 && mmSongInOoT > 0,
                      "(1) a wide row never crossed onto the other game's song host: the predicate keeps it home");
        }
    }

    // ---- (2) A FORCED CROSSING, and its refusal ---------------------------------
    // Three OoT wide rows for two OoT song hosts: one must sit on MM's song host on
    // every seed. With that host refusing foreign items no seed can fit them, so the
    // fill refuses up front. Catches: the crossing branch lost, and capacity counted
    // without the hostAcceptsForeign answer.
    {
        int crossed = 0;
        int failed = 0;
        const ComboLogicBagItem bag[3] = { ClBagItem(O, kOotHook, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(O, kOotLens, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(O, kOotBoots, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t masks[3] = { kClWide, kClWide, kClWide };
        for (int sd = 1; sd <= kSeeds; ++sd) {
            ClBuildSongWorld(kClWide);
            ComboLogicFillResult fr;
            if (ClRunSongFill(bag, 3, masks, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15421000u + (uint32_t)sd, &fr) !=
                RSBS_COMBO_LOGIC_OK) {
                failed++;
                continue;
            }
            GameId g = GAME_NONE;
            const int onMM = (ClHostOfItem(O, kOotHook, &g) == 20 && g == GAME_MM) +
                             (ClHostOfItem(O, kOotLens, &g) == 20 && g == GAME_MM) +
                             (ClHostOfItem(O, kOotBoots, &g) == 20 && g == GAME_MM);
            crossed += (onMM == 1) ? 1 : 0;
        }
        CLS_CHECK(failed == 0 && crossed == kSeeds, "(2) three OoT wide rows for two OoT song hosts did not put "
                                                    "exactly one on MM's song host on every seed");
        ClBuildSongWorld(kClWide);
        gClMM.withAcceptsForeign = true;
        gClMM.foreignRefused = 1u; // host 20
        ClInstall();
        ComboLogicFillResult fr;
        const int rc = ClRunSongFill(bag, 3, masks, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15421100u, &fr);
        ComboLogicGroupShortfall sf;
        const bool named = Combo_Logic_LastGroupShortfall(&sf);
        CLS_CHECK(rc == RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY && fr.attempts == 0 && named &&
                      sf.game == (uint8_t)GAME_OOT && sf.mask == kClWide && sf.rows == 3 && sf.hosts == 2 &&
                      sf.rowsOoT == 3,
                  "(2) with MM's song host refusing foreign items, 3 OoT songs for 2 OoT song hosts must be refused up "
                  "front naming OoT (3 rows, 2 hosts)");
    }

    // ---- (3) THE EXCLUSIVE GROUPS' non-vacuity -----------------------------------
    // With the exclusive group cleared, an unconfined row does land on a song host
    // on some seed: so (1)'s "never" is the exclusive branch's doing.
    {
        int onSong = 0;
        for (int sd = 1; sd <= kSeeds; ++sd) {
            ClBuildSongWorld(kClWide);
            const ComboLogicBagItem bag[2] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                               ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION) };
            ComboLogicFillResult fr;
            if (ClRunSongFill(bag, 2, NULL, 0u, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15422000u + (uint32_t)sd, &fr) ==
                RSBS_COMBO_LOGIC_OK) {
                GameId g = GAME_NONE;
                const int a = ClHostOfItem(O, kOotSword, &g);
                onSong += ClIsSongHost(g, a) ? 1 : 0;
                const int b = ClHostOfItem(M, kMmOcarina, &g);
                onSong += ClIsSongHost(g, b) ? 1 : 0;
            }
        }
        CLS_CHECK(onSong > 0, "(3) control: with no exclusive group no unconfined row ever took a song host, so (1) "
                              "proves nothing");
    }

    // ---- (4) A SONG HOST THAT NEEDS THE ROW ITSELF ---------------------------------
    // OoT song host 11 needs the Hook: under the proving rung the Hook, the only
    // row, is drawn with nothing assumed, so it never lands there; under `none` it
    // does on some seed (the control). Catches the wide row drawn from unreached
    // hosts under a proving rung.
    {
        int onEleven = 0;
        int noneOnEleven = 0;
        const ComboLogicBagItem bag[1] = { ClBagItem(O, kOotHook, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t masks[1] = { kClWide };
        for (int sd = 1; sd <= kSeeds; ++sd) {
            const uint8_t rungs[2] = { (uint8_t)RSBS_COMBO_RUNG_BEATABLE, (uint8_t)RSBS_COMBO_RUNG_NONE };
            for (int r = 0; r < 2; ++r) {
                ClBuildSongWorld(kClWide);
                gClOoT.hostRequires[1] = ClBit(&gClOoT, kOotHook);
                ClInstall();
                ComboLogicFillResult fr;
                if (ClRunSongFill(bag, 1, masks, kClWide, rungs[r], 0x15423000u + (uint32_t)sd, &fr) ==
                    RSBS_COMBO_LOGIC_OK) {
                    GameId g = GAME_NONE;
                    const bool on11 = ClHostOfItem(O, kOotHook, &g) == 11 && g == GAME_OOT;
                    if (r == 0) {
                        onEleven += on11 ? 1 : 0;
                    } else {
                        noneOnEleven += on11 ? 1 : 0;
                    }
                }
            }
        }
        CLS_CHECK(onEleven == 0, "(4) a wide row was placed behind itself under the proving rung");
        CLS_CHECK(noneOnEleven > 0, "(4) control: under No Logic the row never took host 11, so the leg proves "
                                    "nothing");
    }

    // ---- (5) + (6) HALL'S CONDITION AND ITS WORDS ---------------------------------
    // Catches: a HOME_ONLY wide row counted against the other game's hosts, and a
    // shortfall over both games reported as one game's.
    {
        ClBuildSongWorld(kClWide);
        ComboLogicBagItem home[3] = { ClHomeItem(O, kOotHook), ClHomeItem(O, kOotLens), ClHomeItem(O, kOotBoots) };
        const uint32_t masks[3] = { kClWide, kClWide, kClWide };
        ComboLogicFillResult fr;
        int rc = ClRunSongFill(home, 3, masks, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15424000u, &fr);
        ComboLogicGroupShortfall sf;
        bool named = Combo_Logic_LastGroupShortfall(&sf);
        CLS_CHECK(rc == RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY && fr.attempts == 0 && named &&
                      sf.game == (uint8_t)GAME_OOT && sf.mask == kClWide && sf.rows == 3 && sf.hosts == 2 &&
                      sf.wholeGroup == 1,
                  "(5) three home-only OoT songs for two OoT song hosts (a spare MM one) must be refused as OoT's: "
                  "{OoT, wide, 3, 2}");

        ClBuildSongWorld(kClWide);
        const ComboLogicBagItem four[4] = { ClBagItem(O, kOotHook, RSBS_ITEMCLASS_PROGRESSION),
                                            ClBagItem(O, kOotLens, RSBS_ITEMCLASS_PROGRESSION),
                                            ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                            ClBagItem(M, kMmRemains, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t masks4[4] = { kClWide, kClWide, kClWide, kClWide };
        rc = ClRunSongFill(four, 4, masks4, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15424001u, &fr);
        named = Combo_Logic_LastGroupShortfall(&sf);
        CLS_CHECK(rc == RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY && fr.attempts == 0 && named &&
                      sf.game == (uint8_t)GAME_NONE && sf.rows == 4 && sf.hosts == 3 && sf.rowsOoT == 2 &&
                      sf.rowsMM == 2 && sf.wholeGroup == 1,
                  "(6) four songs for three song hosts of both games must be refused naming both: {none, 4, 3}");

        ClBuildSongWorld(kClWide);
        rc = ClRunSongFill(four, 3, masks4, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15424002u, &fr);
        CLS_CHECK(rc == RSBS_COMBO_LOGIC_OK && !Combo_Logic_LastGroupShortfall(NULL),
                  "(6) control: three songs for three song hosts must fill");
    }

    // ---- (7) THE SHARED PAIR AND THE NAMESPACES ------------------------------------
    // Catches: a wide pair refused or kept on one side, a mask mixing local and wide
    // bits accepted, and a host answering both accepted.
    {
        int onOoT = 0;
        int onMM = 0;
        int off = 0;
        const ComboLogicBagItem pair[1] = { ClPairRow() };
        const uint32_t wide[1] = { kClWide };
        for (int sd = 1; sd <= kSeeds; ++sd) {
            ClBuildSongWorld(kClWide);
            ComboLogicFillResult fr;
            if (ClRunSongFill(pair, 1, wide, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15425000u + (uint32_t)sd, &fr) !=
                RSBS_COMBO_LOGIC_OK) {
                off++;
                continue;
            }
            GameId g = GAME_NONE;
            int h = ClHostOfItem(O, kOotPairMask, &g);
            if (h < 0) {
                h = ClHostOfItem(M, kMmPairMask, &g);
            }
            off += ClIsSongHost(g, h) ? 0 : 1;
            onOoT += (g == GAME_OOT) ? 1 : 0;
            onMM += (g == GAME_MM) ? 1 : 0;
        }
        CLS_CHECK(off == 0 && onOoT > 0 && onMM > 0,
                  "(7) a wide shared pair must fill, only on song hosts, on either game's over the seeds");

        ClBuildSongWorld(kClWide);
        const ComboLogicBagItem row[1] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t mixed[1] = { kClWide | 0x1u };
        ComboLogicFillResult fr;
        CLS_CHECK(ClRunSongFill(row, 1, mixed, 0u, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15425100u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(7) a mask with a local and a wide bit must be refused");

        ClBuildSongWorld(kClWide);
        gClMM.hostGroup[1] = kClWide | 0x1u; // host 21 answers both namespaces
        ClInstall();
        CLS_CHECK(ClRunSongFill(row, 1, wide, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15425101u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(7) a host answering a local and a wide group together must be refused");
    }

    // ---- (8) THE DECLARATION ----------------------------------------------------
    // Catches: a wide row or an exclusive group accepted although an engine cannot
    // answer it (a half-armed pair), and a local bit accepted as exclusive.
    {
        const ComboLogicBagItem row[1] = { ClBagItem(O, kOotHook, RSBS_ITEMCLASS_PROGRESSION) };
        const uint32_t wide[1] = { kClWide };
        ComboLogicFillResult fr;
        ClBuildSongWorld(0u);
        CLS_CHECK(ClRunSongFill(row, 1, wide, 0u, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15426000u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(8) a wide row with no engine declaring wide groups must be refused");
        ClBuildSongWorld(kClWide);
        gClMM.wideDeclared = 0u; // MM's half not armed
        ClInstall();
        CLS_CHECK(ClRunSongFill(row, 1, wide, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15426001u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(8) a wide row one engine does not declare must be refused");
        ClBuildSongWorld(kClWide);
        CLS_CHECK(ClRunSongFill(row, 1, NULL, kClWide2, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15426002u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(8) an exclusive group neither engine declares must be refused");
        CLS_CHECK(ClRunSongFill(row, 1, NULL, 0x1u, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15426003u, &fr) ==
                      RSBS_COMBO_LOGIC_ERR_BAD_REQUEST,
                  "(8) a local bit as an exclusive group must be refused");
        CLS_CHECK(ClRunSongFill(row, 1, wide, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15426004u, &fr) ==
                      RSBS_COMBO_LOGIC_OK,
                  "(8) control: both engines declaring the group, the row fills");
    }

    // ---- (9) A LOCALLY CONFINED ROW IS UNMOVED -------------------------------------
    // The #1531 path: an MM row confined to MM's local group places identically with
    // no exclusive group and with an exclusive wide group that holds no host.
    // Catches: the wide branch or the exclusive branch reached by a local row.
    {
        int differ = 0;
        for (int sd = 1; sd <= kSeeds; ++sd) {
            const ComboLogicBagItem bag[2] = { ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                               ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION) };
            const uint32_t masks[2] = { 0x1u, 0u };
            ComboLogicFillResult a;
            ComboLogicFillResult b;
            ClBuildSongWorld(kClWide | kClWide2);
            gClMM.hostGroup[2] = 0x1u; // host 22 in MM's local group
            gClMM.hostGroup[0] = 0u;
            gClOoT.hostGroup[0] = 0u;
            gClOoT.hostGroup[1] = 0u;
            ClInstall();
            const int ra =
                ClRunSongFill(bag, 2, masks, 0u, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15427000u + (uint32_t)sd, &a);
            const int ha = ClHostOfItem(M, kMmBow, NULL);
            ClInstall();
            const int rb =
                ClRunSongFill(bag, 2, masks, kClWide2, (uint8_t)RSBS_COMBO_RUNG_NONE, 0x15427000u + (uint32_t)sd, &b);
            const int hb = ClHostOfItem(M, kMmBow, NULL);
            differ += (ra != RSBS_COMBO_LOGIC_OK || rb != RSBS_COMBO_LOGIC_OK || ha != 22 || hb != 22 ||
                       a.placementDigest != b.placementDigest)
                          ? 1
                          : 0;
        }
        CLS_CHECK(differ == 0, "(9) a locally confined row placed differently under an empty exclusive wide group");
    }

    // ---- (10) A MASK-FREE FILL NEVER ASKS ------------------------------------------
    // NULL masks and all-zero masks with no exclusive group: identical placements,
    // and neither `hostGroups` nor `wideGroups` is called. Catches the arming block
    // entered for a mask-free request (the ABI-8-identical path lost).
    {
        int differ = 0;
        int asked = 0;
        for (int sd = 1; sd <= kSeeds; ++sd) {
            const ComboLogicBagItem bag[3] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                               ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION),
                                               ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION) };
            const uint32_t zeros[3] = { 0u, 0u, 0u };
            ComboLogicFillResult a;
            ComboLogicFillResult b;
            ClBuildSongWorld(kClWide);
            (void)ClRunSongFill(bag, 3, NULL, 0u, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x15428000u + (uint32_t)sd, &a);
            ClInstall();
            (void)ClRunSongFill(bag, 3, zeros, 0u, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x15428000u + (uint32_t)sd, &b);
            differ += (a.status != RSBS_COMBO_LOGIC_OK || a.placementDigest != b.placementDigest) ? 1 : 0;
            asked += gClOoT.hostGroupCalls + gClMM.hostGroupCalls + gClOoT.wideGroupCalls + gClMM.wideGroupCalls;
        }
        CLS_CHECK(differ == 0, "(10) NULL masks and all-zero masks placed differently");
        CLS_CHECK(asked == 0, "(10) a mask-free fill asked hostGroups or wideGroups");
    }

    // ---- (11) THE HOME-ONLY ROWS GO FIRST (#1542, ComboLogicConfinedFirst) --------
    // One game's 12 wide rows are HOME_ONLY and exactly fill its 12 wide hosts; the
    // other game's 14 wide rows may cross and exactly fill its own 14, but every
    // home-game wide host accepts them too; one unconfined row per game for the 2+2
    // plain hosts. Hall's condition holds (12 <= 12, 14 <= 26, 26 <= 26), so the
    // capacity check passes, and only the ORDER decides: the fill succeeds iff no
    // crossing row ever takes a home-game wide host. The three-way partition places
    // all 12 home-only rows first, each seeing only its own side's free wide hosts
    // (never empty until all 12 are placed), so the crossing rows then see only
    // their own side's 14 and the free rows only the plain hosts: OK on EVERY seed,
    // by construction, not by luck. The old two-way partition kept the confined rows
    // in plain shuffled order; over a uniform interleaving and uniform draws its
    // success probability is 1235138341789679 / 400924658534400000 (about 0.31%) per
    // seed, and a replay of ComboLogicShuffle's and ComboLogicDrawAndPlace's exact
    // RNG over the old order dead-ends on all 32 seeds of each direction below (on
    // the 15th to 26th row placed). No retry can rescue a seed: the `none` rung runs ONE
    // attempt (ComboLogicFillNoLogic), asserted below, so the leg does not depend on
    // the batch retries at all. Run twice: OoT home-only, then the mirror image.
    // Catches: the own-side rows no longer placed ahead of the crossing ones (the
    // pre-07467f284 order), and a ConfinedToOwnSide that misses HOME_ONLY.
    {
        const int kHome = 12;
        const int kRoam = 14;
        const int kStarveRows = kHome + kRoam + 2;
        for (int dir = 0; dir < 2; ++dir) {
            const uint8_t homeGame = (dir == 0) ? O : M;
            const uint8_t roamGame = (dir == 0) ? M : O;
            const int ootWide = (dir == 0) ? kHome : kRoam;
            const int mmWide = (dir == 0) ? kRoam : kHome;
            const uint16_t homeBaseId = (dir == 0) ? 120 : 220;
            const uint16_t roamBaseId = (dir == 0) ? 220 : 120;
            const uint32_t seedBase = (dir == 0) ? 0x15429000u : 0x15429100u;
            // Bag order (home-only, crossing, the two free rows) is what the replay's
            // seed list was computed against: the shuffle permutes bag indices.
            ComboLogicBagItem bag[kStarveRows];
            uint32_t masks[kStarveRows];
            for (int i = 0; i < kHome; ++i) {
                bag[i] = ClHomeItem(homeGame, (uint16_t)(homeBaseId + i));
                masks[i] = kClWide;
            }
            for (int i = 0; i < kRoam; ++i) {
                bag[kHome + i] = ClBagItem(roamGame, (uint16_t)(roamBaseId + i), RSBS_ITEMCLASS_PROGRESSION);
                masks[kHome + i] = kClWide;
            }
            bag[kHome + kRoam] = ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION);
            masks[kHome + kRoam] = 0u;
            bag[kHome + kRoam + 1] = ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION);
            masks[kHome + kRoam + 1] = 0u;

            int failed = 0;
            int miscounted = 0;
            int homeOk = 0;
            int roamOk = 0;
            int freeOk = 0;
            int unknown = 0;
            for (int sd = 1; sd <= kSeeds; ++sd) {
                ClBuildStarveWorld(ootWide, mmWide);
                ComboLogicFillResult fr;
                const int rc = ClRunSongFill(bag, kStarveRows, masks, kClWide, (uint8_t)RSBS_COMBO_RUNG_NONE,
                                             seedBase + (uint32_t)sd, &fr);
                if (rc != RSBS_COMBO_LOGIC_OK) {
                    failed++;
                    continue;
                }
                const int counted = Combo_Logic_PlacementCount(GAME_OOT) + Combo_Logic_PlacementCount(GAME_MM);
                miscounted += (fr.attempts != 1 || fr.requiredPlaced != kStarveRows || counted != kStarveRows) ? 1 : 0;
                const GameId both[2] = { GAME_OOT, GAME_MM };
                for (int g = 0; g < 2; ++g) {
                    for (int p = 0; p < Combo_Logic_PlacementCount(both[g]); ++p) {
                        ComboLogicPlacement pl;
                        if (!Combo_Logic_PlacementAt(both[g], p, &pl)) {
                            unknown++;
                            continue;
                        }
                        const bool onWide = ClIsStarveWideHost(both[g], pl.hostCheck, ootWide, mmWide);
                        const uint8_t origin = pl.item.originGame;
                        const uint16_t id = pl.item.id;
                        if (origin == homeGame && id >= homeBaseId && id < homeBaseId + kHome) {
                            homeOk += (onWide && (uint8_t)both[g] == homeGame) ? 1 : 0;
                        } else if (origin == roamGame && id >= roamBaseId && id < roamBaseId + kRoam) {
                            roamOk += onWide ? 1 : 0;
                        } else if ((origin == O && id == kOotSword) || (origin == M && id == kMmOcarina)) {
                            freeOk += onWide ? 0 : 1;
                        } else {
                            unknown++;
                        }
                    }
                }
            }
            printf("[TEST] (11) %s home-only: %d failed, %d miscounted, home-only rows on own wide hosts %d of %d, "
                   "crossing rows on wide hosts %d of %d, free rows on plain hosts %d of %d, %d unknown\n",
                   dir == 0 ? "OoT" : "MM", failed, miscounted, homeOk, kHome * kSeeds, roamOk, kRoam * kSeeds, freeOk,
                   2 * kSeeds, unknown);
            // The old order dead-ends here on every seed (ERR_NO_CANDIDATE for a
            // home-only row whose last host a crossing row took).
            CLS_CHECK(failed == 0, "(11) a fill failed: a crossing wide row took a home-only row's only host (the "
                                   "confined rows were not placed own-side first)");
            // A fill that returned OK without placing every row, or that took a
            // second attempt (the leg would then lean on retry luck).
            CLS_CHECK(miscounted == 0, "(11) a fill took more than one attempt or placed a count other than the bag's");
            // A home-only row on the other game, or off the wide hosts.
            CLS_CHECK(homeOk == kHome * kSeeds, "(11) a home-only wide row sat off its own game's wide hosts");
            // A crossing wide row on a plain host (its confinement lost).
            CLS_CHECK(roamOk == kRoam * kSeeds, "(11) a crossing wide row sat off the wide hosts");
            // An unconfined row on a wide host (the exclusive group lost), or a
            // placement no row of the bag accounts for.
            CLS_CHECK(freeOk == 2 * kSeeds && unknown == 0,
                      "(11) an unconfined row took a wide host, or a placement matched no row of the bag");
        }
        // Back to the shape the row's other legs build.
        ClBuildSongWorld(kClWide);
    }

    ClUninstall();
    printf("[TEST] combo-logic-song-pool: %s\n", sClsFailures == 0 ? "PASS" : "FAIL");
    return sClsFailures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// combo-logic-travel-facts — THE TRAVEL FACTS (ABI 10, #1169 XW9)
// ============================================================================

namespace {

/**
 * THE TRAVEL WORLD. Each game holds one host behind the OTHER game's travel fact:
 *   OoT host 11 needs MM's Minuet fact (0x02), which MM reports while it holds
 *   the Ocarina; MM host 21 needs OoT's Soaring fact (0x01), which OoT reports
 *   while it holds the Boots. OoT's crossing needs the Sword; MM's needs
 *   `mmCrossing` (0 = always open). OoT host 11 holds OoT's Lens (OoT's goal);
 *   MM host 21 holds `mm21` (an MM item, MM's own placement).
 */
void ClBuildTravelWorld(uint32_t mmCrossingItem, uint16_t mm21, bool withFacts) {
    ClResetEngine(&gClOoT, (uint8_t)GAME_OOT);
    ClAddItem(&gClOoT, kOotSword);
    ClAddItem(&gClOoT, kOotHook);
    ClAddItem(&gClOoT, kOotLens);
    ClAddItem(&gClOoT, kOotBoots);
    ClAddHost(&gClOoT, 10, 0u);
    ClAddHost(&gClOoT, 11, 0u);
    ClAddHost(&gClOoT, 12, 0u);
    gClOoT.hostFactRequires[1] = 0x02u;
    gClOoT.crossingRequires = ClBit(&gClOoT, kOotSword);
    gClOoT.goalRequires = ClBit(&gClOoT, kOotLens);
    gClOoT.withTravelFacts = withFacts;
    gClOoT.factEnabled = 0x01u;
    gClOoT.factRequires[0] = ClBit(&gClOoT, kOotBoots);
    gClOoT.withholdCalls = 0;

    ClResetEngine(&gClMM, (uint8_t)GAME_MM);
    ClAddItem(&gClMM, kMmOcarina);
    ClAddItem(&gClMM, kMmBow);
    ClAddItem(&gClMM, kMmRemains);
    ClAddItem(&gClMM, kMmMask);
    ClAddHost(&gClMM, 20, 0u);
    ClAddHost(&gClMM, 21, 0u);
    ClAddHost(&gClMM, 22, 0u);
    gClMM.hostFactRequires[1] = 0x01u;
    gClMM.crossingRequires = (mmCrossingItem != 0u) ? ClBit(&gClMM, (uint16_t)mmCrossingItem) : 0u;
    gClMM.goalRequires = ClBit(&gClMM, kMmRemains);
    gClMM.withTravelFacts = withFacts;
    gClMM.factEnabled = 0x02u;
    gClMM.factRequires[1] = ClBit(&gClMM, kMmOcarina);
    ClInstall();
    (void)Combo_Logic_Place(GAME_OOT, 11, ClItem((uint8_t)GAME_OOT, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
    if (mm21 != 0u) {
        (void)Combo_Logic_Place(GAME_MM, 21, ClItem((uint8_t)GAME_MM, mm21), RSBS_ITEMCLASS_PROGRESSION);
    }
}

/** One beat-either round over the travel world with `n` assumed items. */
int ClTravelRound(const ComboLogicBagItem* assumed, int n, ComboLogicRoundResult* out) {
    memset(out, 0, sizeof(*out));
    return ClRunRound((uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER, assumed, n, out);
}

void ClPrintTravelRound(const char* tag, int rc, const ComboLogicRoundResult& r) {
    printf("[TEST]   %s: rc=%s iterations=%d cross OoT %d MM %d goal OoT %d MM %d travel toOoT %02X toMM %02X | "
           "accepts OoT %d MM %d | reached OoT %03X MM %03X\n",
           tag, Combo_Logic_StatusName(rc), r.iterations, r.crossingOpenOoT, r.crossingOpenMM, r.goalOoT, r.goalMM,
           (unsigned)r.travelToOoT, (unsigned)r.travelToMM, gClOoT.acceptCalls, gClMM.acceptCalls,
           (unsigned)gClOoT.reached, (unsigned)gClMM.reached);
}

} // namespace

// A SOFT check, like CLG_CHECK: every leg runs after one fails.
static int sCltFailures;
#define CLT_CHECK(cond, msg)                                                \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[TEST] FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            sCltFailures++;                                                 \
        }                                                                   \
    } while (0)

TestResult Test_ComboLogicTravelFacts(void) {
    printf("[TEST] combo-logic-travel-facts: an MM warp-song fact reaches OoT only with OoT's crossing open, OoT's "
           "Soaring fact reaches MM only with both crossings open, chains converge, a vanished fact is non-monotone, "
           "own-direction bits are dropped, and silent engines change nothing (ABI 10, #1169 XW9)\n");
    const uint8_t O = (uint8_t)GAME_OOT;
    const uint8_t M = (uint8_t)GAME_MM;
    sCltFailures = 0;
    ClUninstall();
    ComboLogicRoundResult r;

    // ---- T1: the trailing entries default to NULL, and the ABI is 10 -----------
    // Catches: a vtable initialiser that no longer zero-fills the new entries, or
    // the ABI number not moving with the contract. The mismatch refusal itself is
    // combo-logic-engine-surface's (ABI + 7).
    {
        ComboLogicEngine vt;
        ClResetEngine(&gClOoT, O);
        ClFillVtable(&vt, &gClOoT);
        CLT_CHECK(RSBS_COMBO_LOGIC_ENGINE_ABI == 11u,
                  "T1: this build must speak ABI 11 (travel facts at 10, the combo-wide groups at 11)");
        CLT_CHECK(vt.travelFacts == NULL && vt.acceptTravelFacts == NULL && vt.wideGroups == NULL,
                  "T1: an ABI-9-shaped stub must leave both travel entries and the wide-groups entry NULL");
        CLT_CHECK(Combo_Logic_RegisterEngine(GAME_OOT, &vt), "T1: an engine without travel entries must register");
        CLT_CHECK(Combo_Logic_RegisterEngine(GAME_OOT, NULL), "T1: un-registration");
    }

    // ---- T0: the gate as a pure function ----------------------------------------
    // Catches: a gate that forgets a crossing term, or that lets an engine's own
    // direction through.
    {
        uint32_t toOoT = 0xFFu;
        uint32_t toMM = 0xFFu;
        Combo_Logic_TravelGate(0x7Fu, 0x7Fu, 1, 1, &toOoT, &toMM);
        CLT_CHECK(toOoT == 0x7Eu && toMM == 0x01u, "T0: both crossings open must pass each direction, masked");
        Combo_Logic_TravelGate(0x7Fu, 0x7Fu, 0, 1, &toOoT, &toMM);
        CLT_CHECK(toOoT == 0u && toMM == 0u, "T0: OoT's crossing closed must pass nothing either way");
        Combo_Logic_TravelGate(0x7Fu, 0x7Fu, 1, 0, &toOoT, &toMM);
        CLT_CHECK(toOoT == 0x7Eu && toMM == 0u, "T0: MM's crossing closed must still pass MM's songs, not Soaring");
        Combo_Logic_TravelGate(0x7Eu, 0x01u, 1, 1, &toOoT, &toMM);
        CLT_CHECK(toOoT == 0u && toMM == 0u, "T0: an engine's report in its own direction must be dropped");
        Combo_Logic_TravelGate(0x01u, 0x7Eu, 1, 1, NULL, NULL); // NULL outs are legal
    }

    // ---- T2: an MM fact reaches OoT only with OoT's crossing open ----------------
    // Catches: the toOoT gate missing (Sword removed, the Lens still counts), the
    // fact never delivered (with everything, the Lens does not count), or a fact
    // credited without its item (Ocarina removed).
    {
        const ComboLogicBagItem all[2] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION) };
        ClBuildTravelWorld(0u, 0u, true);
        int rc = ClTravelRound(all, 2, &r);
        ClPrintTravelRound("T2 Sword + Ocarina", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalOoT == 1 && r.travelToOoT == 0x02u && r.travelToMM == 0u,
                  "T2: MM's Minuet fact must open OoT host 11 once OoT's crossing is open");
        CLT_CHECK(gClOoT.acceptCalls == 1 && !gClOoT.sawAcceptShrink && gClMM.acceptCalls == 0,
                  "T2: OoT must be handed the fact exactly once, MM never");

        ClBuildTravelWorld(0u, 0u, true);
        rc = ClTravelRound(&all[1], 1, &r);
        ClPrintTravelRound("T2 Ocarina only (crossing closed)", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalOoT == 0 && r.travelToOoT == 0u && gClOoT.acceptCalls == 0,
                  "T2: with OoT's crossing closed no MM fact may reach OoT (a warp is never a first way in)");

        ClBuildTravelWorld(0u, 0u, true);
        rc = ClTravelRound(all, 1, &r);
        ClPrintTravelRound("T2 Sword only", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalOoT == 0 && r.travelToOoT == 0u,
                  "T2: without the Ocarina MM reports no fact and OoT host 11 stays unreached");
        CLT_CHECK(ClContractClean(), "T2: contract");
    }

    // ---- T3: the Soaring fact needs both crossings -------------------------------
    // Catches: the toMM gate missing MM's crossing (Bow removed) or OoT's (Sword
    // removed), or the fact not delivered at all.
    {
        const ComboLogicBagItem all[3] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(O, kOotBoots, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(M, kMmBow, RSBS_ITEMCLASS_PROGRESSION) };
        ClBuildTravelWorld(kMmBow, kMmRemains, true);
        int rc = ClTravelRound(all, 3, &r);
        ClPrintTravelRound("T3 Sword + Boots + Bow", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalMM == 1 && r.travelToMM == 0x01u && r.travelToOoT == 0u,
                  "T3: OoT's Soaring fact must open MM host 21 once both crossings are open");

        ClBuildTravelWorld(kMmBow, kMmRemains, true);
        rc = ClTravelRound(all, 2, &r);
        ClPrintTravelRound("T3 no Bow (MM's crossing closed)", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalMM == 0 && r.travelToMM == 0u && gClMM.acceptCalls == 0,
                  "T3: with MM's crossing closed (never departed) the Soaring fact must not reach MM");

        ClBuildTravelWorld(kMmBow, kMmRemains, true);
        rc = ClTravelRound(&all[1], 2, &r);
        ClPrintTravelRound("T3 no Sword (OoT's crossing closed)", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalMM == 0 && r.travelToMM == 0u,
                  "T3: with OoT's crossing closed the Soaring fact must not reach MM");
        CLT_CHECK(ClContractClean(), "T3: contract");
    }

    // ---- T4 + T8: a three-alternation chain, in both item orders -----------------
    // OoT's Boots -> Soaring fact -> MM host 21 (MM's Ocarina) -> Minuet fact -> OoT
    // host 11 (the Lens). Catches: facts read only once per round (the chain stops
    // at MM host 21), an accept that is not a superset or is repeated, facts that
    // leak between rounds, and an order dependence.
    {
        const ComboLogicBagItem fwd[2] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(O, kOotBoots, RSBS_ITEMCLASS_PROGRESSION) };
        const ComboLogicBagItem rev[2] = { fwd[1], fwd[0] };
        ClBuildTravelWorld(0u, kMmOcarina, true);
        int rc = ClTravelRound(fwd, 2, &r);
        ClPrintTravelRound("T4 chain", rc, r);
        const ComboLogicRoundResult first = r;
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalOoT == 1 && r.travelToMM == 0x01u && r.travelToOoT == 0x02u,
                  "T4: the chain OoT fact -> MM item -> MM fact -> OoT host must converge to the Lens");
        CLT_CHECK(r.iterations >= 4, "T4: the chain needs at least four alternations (three growths and a quiet one)");
        CLT_CHECK(gClOoT.acceptCalls == 1 && gClMM.acceptCalls == 1 && !gClOoT.sawAcceptShrink &&
                      !gClMM.sawAcceptShrink,
                  "T8: each engine must be handed its one fact exactly once, as a superset");
        CLT_CHECK(gClOoT.accepted == 0u && gClMM.accepted == 0u, "T8: endQuery must forget the accepted facts");
        rc = ClTravelRound(fwd, 2, &r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalOoT == 1 && r.iterations == first.iterations &&
                      gClOoT.acceptCalls == 2 && gClMM.acceptCalls == 2,
                  "T8: a second round must start from no facts and hand each across again");

        ClBuildTravelWorld(0u, kMmOcarina, true);
        rc = ClTravelRound(rev, 2, &r);
        ClPrintTravelRound("T4 chain, reversed", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.goalOoT == first.goalOoT && r.goalMM == first.goalMM &&
                      r.travelToOoT == first.travelToOoT && r.travelToMM == first.travelToMM &&
                      r.iterations == first.iterations,
                  "T4: the round must not depend on the order of the assumed items");
        CLT_CHECK(ClContractClean(), "T4: contract");
    }

    // ---- T9: a withheld host's item yields no fact -------------------------------
    // Catches: a fact derived from an item the round withholds.
    {
        const ComboLogicBagItem fwd[2] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(O, kOotBoots, RSBS_ITEMCLASS_PROGRESSION) };
        ClBuildTravelWorld(0u, kMmOcarina, true);
        const ComboLogicHostRef forbid[1] = { ClHostRef(M, 21) };
        ComboLogicRoundRequest req;
        memset(&req, 0, sizeof(req));
        req.assumed = fwd;
        req.assumedCount = 2;
        req.goal = (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER;
        req.forbidden = forbid;
        req.forbiddenCount = 1;
        memset(&r, 0, sizeof(r));
        const int rc = Combo_Logic_RunRound(&req, &r);
        ClPrintTravelRound("T9 MM host 21 forbidden", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.travelToMM == 0x01u && r.travelToOoT == 0u && r.goalOoT == 0,
                  "T9: the withheld Ocarina must produce no Minuet fact");
    }

    // ---- T5: a fact that disappears is non-monotone -----------------------------
    {
        ClBuildTravelWorld(0u, 0u, true);
        gClMM.factEnabled = 0x02u;
        gClMM.factRequires[1] = 0u;
        gClMM.factDropAfterFirst = true;
        ClInstall();
        (void)Combo_Logic_Place(GAME_OOT, 11, ClItem(O, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        const ComboLogicBagItem sword[1] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION) };
        const int rc = ClTravelRound(sword, 1, &r);
        ClPrintTravelRound("T5 vanishing fact", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_ERR_NON_MONOTONE, "T5: a fact that disappears inside a round is NON_MONOTONE");
    }

    // ---- T6: own-direction bits are dropped -------------------------------------
    {
        ClBuildTravelWorld(0u, kMmRemains, true);
        gClOoT.factEnabled = 0u;
        gClOoT.factReportExtra = 0x02u; // MM's direction, from OoT
        gClMM.factEnabled = 0u;
        gClMM.factReportExtra = 0x01u; // OoT's direction, from MM
        ClInstall();
        (void)Combo_Logic_Place(GAME_OOT, 11, ClItem(O, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        (void)Combo_Logic_Place(GAME_MM, 21, ClItem(M, kMmRemains), RSBS_ITEMCLASS_PROGRESSION);
        const ComboLogicBagItem sword[1] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION) };
        const int rc = ClTravelRound(sword, 1, &r);
        ClPrintTravelRound("T6 own-direction reports", rc, r);
        CLT_CHECK(rc == RSBS_COMBO_LOGIC_OK && r.travelToOoT == 0u && r.travelToMM == 0u && r.goalOoT == 0 &&
                      r.goalMM == 0 && gClOoT.acceptCalls == 0 && gClMM.acceptCalls == 0,
                  "T6: an engine must never be handed a fact of its own direction");
    }

    // ---- T7: silent engines change nothing ---------------------------------------
    // With both entries NULL, and with them installed but reporting 0, a round and
    // a fill are byte-identical, and nothing is accepted. Catches: an extra
    // alternation, an accept call or any consumed state when every fact is 0 (the
    // rules-off world).
    {
        const ComboLogicBagItem fwd[2] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                           ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION) };
        ComboLogicRoundResult silentNull;
        ComboLogicRoundResult silentZero;
        ClBuildTravelWorld(0u, kMmRemains, false);
        int rcNull = ClTravelRound(fwd, 2, &silentNull);
        ClBuildTravelWorld(0u, kMmRemains, true);
        gClOoT.factEnabled = 0u;
        gClMM.factEnabled = 0u;
        ClInstall();
        (void)Combo_Logic_Place(GAME_OOT, 11, ClItem(O, kOotLens), RSBS_ITEMCLASS_PROGRESSION);
        (void)Combo_Logic_Place(GAME_MM, 21, ClItem(M, kMmRemains), RSBS_ITEMCLASS_PROGRESSION);
        int rcZero = ClTravelRound(fwd, 2, &silentZero);
        CLT_CHECK(rcNull == RSBS_COMBO_LOGIC_OK && rcZero == RSBS_COMBO_LOGIC_OK &&
                      memcmp(&silentNull, &silentZero, sizeof(silentNull)) == 0,
                  "T7: a round with facts all 0 must be byte-identical to one with no travel entries");
        CLT_CHECK(gClOoT.travelCalls > 0 && gClMM.travelCalls > 0 && gClOoT.acceptCalls == 0 && gClMM.acceptCalls == 0,
                  "T7: the silent engines were asked (non-vacuity) and handed nothing");

        // The same comparison over a fill (placement digest, rounds, attempts).
        ComboLogicBagItem bag[3] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                     ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION),
                                     ClBagItem(O, kOotLens, RSBS_ITEMCLASS_PROGRESSION) };
        ComboLogicFillResult fNull;
        ComboLogicFillResult fZero;
        ClBuildTravelWorld(0u, 0u, false);
        Combo_Logic_ResetPlacements();
        rcNull =
            ClRunFill(bag, 3, (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x0B0Eu, &fNull);
        ClBuildTravelWorld(0u, 0u, true);
        gClOoT.factEnabled = 0u;
        gClMM.factEnabled = 0u;
        ClInstall();
        rcZero =
            ClRunFill(bag, 3, (uint8_t)RSBS_COMBO_GOAL_BEAT_EITHER, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x0B0Eu, &fZero);
        printf("[TEST]   T7 fill: NULL rc=%s digest %08X rounds %d | zero rc=%s digest %08X rounds %d\n",
               Combo_Logic_StatusName(rcNull), (unsigned)fNull.placementDigest, fNull.rounds,
               Combo_Logic_StatusName(rcZero), (unsigned)fZero.placementDigest, fZero.rounds);
        CLT_CHECK(rcNull == rcZero && fNull.placementDigest == fZero.placementDigest && fNull.rounds == fZero.rounds &&
                      fNull.attempts == fZero.attempts && fNull.placed == fZero.placed,
                  "T7: a fill with facts all 0 must place exactly as one with no travel entries");
        CLT_CHECK(gClOoT.acceptCalls == 0 && gClMM.acceptCalls == 0, "T7: the fill handed no fact across");
    }

    // ---- T10: a fill whose only goal route is a fact -------------------------------
    // OoT's goal (the Lens) is placeable only on OoT host 11 (behind MM's Minuet
    // fact, which needs MM's Ocarina and OoT's crossing, the Sword) or the two
    // sphere-zero hosts. Catches: a fact read before it is earned (the Ocarina or
    // the Sword drawn onto host 11, which only they open), and, with the facts
    // off, a proof that credits host 11 at all.
    {
        // One host per side so every bag row needs host 11 or an honest route.
        ComboLogicBagItem bag[3] = { ClBagItem(O, kOotSword, RSBS_ITEMCLASS_PROGRESSION),
                                     ClBagItem(M, kMmOcarina, RSBS_ITEMCLASS_PROGRESSION),
                                     ClBagItem(O, kOotLens, RSBS_ITEMCLASS_PROGRESSION) };
        int ok = 0;
        int lensOn11 = 0;
        int keyOn11 = 0;
        const int kSeeds = 16;
        for (int sd = 0; sd < kSeeds; ++sd) {
            ClResetEngine(&gClOoT, O);
            ClAddItem(&gClOoT, kOotSword);
            ClAddItem(&gClOoT, kOotLens);
            ClAddHost(&gClOoT, 10, 0u);
            ClAddHost(&gClOoT, 11, 0u);
            gClOoT.hostFactRequires[1] = 0x02u;
            gClOoT.crossingRequires = ClBit(&gClOoT, kOotSword);
            gClOoT.goalRequires = ClBit(&gClOoT, kOotLens);
            gClOoT.withTravelFacts = true;
            ClResetEngine(&gClMM, M);
            ClAddItem(&gClMM, kMmOcarina);
            ClAddHost(&gClMM, 20, 0u);
            gClMM.goalRequires = 0u;
            gClMM.withTravelFacts = true;
            gClMM.factEnabled = 0x02u;
            gClMM.factRequires[1] = ClBit(&gClMM, kMmOcarina);
            ClInstall();
            ComboLogicFillResult fr;
            const int rc = ClRunFill(bag, 3, (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, (uint8_t)RSBS_COMBO_RUNG_BEATABLE,
                                     0x10A0u + (uint32_t)sd, &fr);
            if (rc != RSBS_COMBO_LOGIC_OK || !fr.goalProven) {
                continue;
            }
            ok++;
            for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
                ComboLogicPlacement p;
                if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.hostCheck == 11) {
                    lensOn11 += (p.item.id == kOotLens) ? 1 : 0;
                    keyOn11 += (p.item.id == kOotSword || p.item.id == kMmOcarina) ? 1 : 0;
                }
            }
        }
        printf("[TEST]   T10: %d/%d fills proved, the Lens on host 11 %d time(s), a key item on it %d time(s)\n", ok,
               kSeeds, lensOn11, keyOn11);
        CLT_CHECK(ok > 0, "T10: no fill proved a goal whose only third host is behind a fact");
        CLT_CHECK(keyOn11 == 0, "T10: the Sword or the Ocarina was placed behind the fact it opens");
        CLT_CHECK(lensOn11 == ok, "T10: every proved fill must put the Lens on host 11, the only honest layout");

        // The facts OFF (NULL entries): host 11 is never reached, so the bag cannot
        // be proved. The control that the proof above used the fact.
        ClResetEngine(&gClOoT, O);
        ClAddItem(&gClOoT, kOotSword);
        ClAddItem(&gClOoT, kOotLens);
        ClAddHost(&gClOoT, 10, 0u);
        ClAddHost(&gClOoT, 11, 0u);
        gClOoT.hostFactRequires[1] = 0x02u;
        gClOoT.crossingRequires = ClBit(&gClOoT, kOotSword);
        gClOoT.goalRequires = ClBit(&gClOoT, kOotLens);
        ClResetEngine(&gClMM, M);
        ClAddItem(&gClMM, kMmOcarina);
        ClAddHost(&gClMM, 20, 0u);
        gClMM.goalRequires = 0u;
        ClInstall();
        ComboLogicFillResult fr;
        const int rcOff =
            ClRunFill(bag, 3, (uint8_t)RSBS_COMBO_GOAL_BEAT_BOTH, (uint8_t)RSBS_COMBO_RUNG_BEATABLE, 0x10A0u, &fr);
        printf("[TEST]   T10 control (no travel entries): rc=%s goalProven %d\n", Combo_Logic_StatusName(rcOff),
               fr.goalProven ? 1 : 0);
        CLT_CHECK(rcOff != RSBS_COMBO_LOGIC_OK, "T10: without the facts the same bag must not be provable");
    }

    ClUninstall();
    printf("[TEST] combo-logic-travel-facts: %s\n", sCltFailures == 0 ? "PASS" : "FAIL");
    return sCltFailures == 0 ? TEST_PASS : TEST_FAIL;
}
