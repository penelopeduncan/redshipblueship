/**
 * @file combo_single_bag.c
 * @brief The single-bag fill at the creation event (see combo_single_bag.h).
 *
 * Game-header-free like combo_logic.c: every game fact arrives through the two
 * ports' extern "C" halves declared in the header, as game-neutral scalars.
 */

#include "combo_single_bag.h"

#include "combo_song_pool.h" // #1542: the shared song pool's one predicate
#include "combo_woth.h"      // #1439: the Way of the Hero the proved world is analysed for
#include "context.h"
#include "foreign_items.h"
#include "gen_budget.h"
#include "gen_progress_overlay.h"
#include "shared_items.h"
#include "shared_resources.h" // the shared-family registry THE SHARED PAIR reads (#976)
#include "triforce_hunt.h"    // #1389: the frozen combo requirement a triforce-hunt fill proves

#include <stdio.h>
#include <string.h>

// ============================================================================
// Buffers. Static, like every bag-sized buffer in combo_logic.c: the fill runs on
// the game thread inside the creation bracket, and these are far past any sane
// frame budget.
// ============================================================================

/** Pool rows of both games in one fill: OoT's general-pass pool (a few hundred
 *  rows) plus MM's GeneratePools pool (283 on the shipped profile, the whole
 *  graph's ~2250 checks' worth on a profile that shuffles every location). */
#define COMBO_SINGLE_BAG_ROW_CAP 8192

static ComboLogicPoolRow sRows[COMBO_SINGLE_BAG_ROW_CAP];
static uint16_t sOoTItems[COMBO_SINGLE_BAG_ROW_CAP];
static uint16_t sOoTFlags[COMBO_SINGLE_BAG_ROW_CAP];
static ComboLogicBagItem sBag[RSBS_COMBO_LOGIC_BAG_CAP];
static int sBagPoolIndex[RSBS_COMBO_LOGIC_BAG_CAP];
/** THE HOST GROUPS (combo_logic.h ABI 9, #1502): each bag row's confinement mask,
 *  handed to the fill only when one is nonzero. */
static uint32_t sBagConfine[RSBS_COMBO_LOGIC_BAG_CAP];
/** #1542: OoT's pool rows' masks (OoT_ComboLogic_PoolRowConfineMasks), one per
 *  exported row: the shared song pool's wide group for a song, else 0. */
static uint32_t sOoTConfine[COMBO_SINGLE_BAG_ROW_CAP];
static int sOoTBagRows[RSBS_COMBO_LOGIC_BAG_CAP];
/** The pool rows (indices into sRows) THE SHARED-QUANTITY TRIM removed, captured
 *  right after the compose (Combo_Logic_ComposeTrimmedAt's record is the
 *  composer's, and the next compose overwrites it). */
static int sTrimmedPoolRows[RSBS_COMBO_LOGIC_BAG_CAP];
static int sTrimmedCount;
static int sOoTTrimmedRows[RSBS_COMBO_LOGIC_BAG_CAP];

static ComboSingleBagReport sReport;

const ComboSingleBagReport* Combo_SingleBag_LastReport(void) {
    return &sReport;
}

// ============================================================================
// The seed
// ============================================================================

static void SingleBagFold(uint32_t* h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        *h ^= (v >> (8 * i)) & 0xFFu;
        *h *= 16777619u;
    }
}

/** FNV-1a over a domain tag and the frozen identity. The tag keeps each stream
 *  disjoint from every other identity-derived stream in the tree (the old
 *  placement passes' xorshifts, MM's attempt mix, each other), so a change to
 *  one can never alias another. */
static uint32_t SingleBagIdentityHash(const char* tag) {
    uint32_t h = 2166136261u;
    for (const char* c = tag; *c != '\0'; ++c) {
        h ^= (uint8_t)*c;
        h *= 16777619u;
    }
    SingleBagFold(&h, gComboCtx.sharedRandoSeed);
    SingleBagFold(&h, gComboCtx.sharedRandoSettingsHash);
    SingleBagFold(&h, gComboCtx.mmProfileDigest);
    // The fingerprint WITHOUT its generator label (#1003): the version is a
    // label, never an entropy input, so a bump of RSBS_GENERATOR_VERSION moves
    // no placement. For every pre-#1003 fingerprint this is the stored value.
    SingleBagFold(&h, Combo_ComboSettingsWorldHash());
    return h;
}

uint32_t Combo_SingleBag_SeedFor(int ladderAttempt) {
    uint32_t h = SingleBagIdentityHash("rsbs-single-bag-v1");
    SingleBagFold(&h, (uint32_t)ladderAttempt);
    return h;
}

uint32_t Combo_SingleBag_TrimSeed(void) {
    // No ladder attempt: which copies of a shared family survive is decided by
    // the identity alone, so every ladder attempt fills the same bag.
    return SingleBagIdentityHash("rsbs-single-bag-trim-v1");
}

uint32_t Combo_SingleBag_FoolishSeed(void) {
    // No ladder attempt either: the analysis reads the world the successful
    // attempt proved, and that world is already a function of the identity.
    return SingleBagIdentityHash("rsbs-foolish-zigzag-v1");
}

// TEST ONLY (epic #575 H4): see Combo_SingleBag_TestSetPostFillProbe.
static ComboSingleBagPostFillProbe sPostFillProbe = NULL;
static void* sPostFillProbeCtx = NULL;

void Combo_SingleBag_TestSetPostFillProbe(ComboSingleBagPostFillProbe probe, void* ctx) {
    sPostFillProbe = probe;
    sPostFillProbeCtx = ctx;
}

// ============================================================================
// The observer: progress, frames, and the per-attempt stop
// ============================================================================

typedef struct {
    int ladderAttempt;
    uint32_t roundBudget;    // the per-attempt round budget (#950); 0 = none
    uint32_t startMs;        // Combo_GenBudget_NowMs() at the fill's start
    uint32_t presentStartMs; // Combo_GenProgress_PresentationMs() at the fill's start
    uint32_t lastReportMs;   // generation ms at the last progress report
    int lastStage;
    int lastAttempt;
    bool aborted;
    char detail[96];
} SingleBagWatch;

/** Generation time since the fill started: wall time minus what the overlay
 *  spent presenting, both in gen_budget's clock. DISPLAY ONLY since #950: it
 *  paces the heartbeat and the caption and is printed, and the stop below counts
 *  rounds instead. */
static uint32_t SingleBagGenerationMs(const SingleBagWatch* w) {
    const uint32_t now = Combo_GenBudget_NowMs();
    const uint32_t wall = (now >= w->startMs) ? now - w->startMs : 0u;
    const uint32_t presentNow = Combo_GenProgress_PresentationMs();
    const uint32_t presented = (presentNow >= w->presentStartMs) ? presentNow - w->presentStartMs : 0u;
    return (wall > presented) ? wall - presented : 0u;
}

static int SingleBagObserve(void* ctx, const ComboLogicFillProgress* p) {
    SingleBagWatch* w = (SingleBagWatch*)ctx;
    const uint32_t genMs = SingleBagGenerationMs(w);

    // Frames first, so a slow round still leaves a live window. The heartbeat is
    // rate-limited to 10 Hz inside the overlay; its clock is this attempt's. The
    // rounds go in first (#1286): the bar and the elapsed line show the share of
    // the per-attempt ROUND budget spent, which is what the stop below compares,
    // not the attempt's clock against a time estimate.
    if (ComboGenOverlay_WantsHeartbeat()) {
        ComboGenOverlay_NoteRounds(p->rounds > 0 ? (uint32_t)p->rounds : 0u, w->roundBudget);
        (void)ComboGenOverlay_Heartbeat(genMs);
    }

    // THE CAPTION: the coordinator's phases, reported on every change of stage
    // or batch attempt and at most once a second in between — every report is a
    // stderr line and a painted frame, and a fill runs a round per bag row.
    const bool stageChanged = p->stage != w->lastStage || p->attempt != w->lastAttempt;
    if (stageChanged || genMs - w->lastReportMs >= 1000u) {
        switch (p->stage) {
            case RSBS_COMBO_FILL_STAGE_PROOF:
                snprintf(w->detail, sizeof(w->detail), "Proving the paired world can be finished");
                break;
            case RSBS_COMBO_FILL_STAGE_SURPLUS:
                snprintf(w->detail, sizeof(w->detail), "Placing the extra copies");
                break;
            case RSBS_COMBO_FILL_STAGE_PREFLIGHT:
                snprintf(w->detail, sizeof(w->detail), "Checking that every location in both worlds is reachable");
                break;
            default:
                snprintf(w->detail, sizeof(w->detail), "Placing item %d of %d in both worlds (round %d, batch %d)",
                         p->requiredPlaced + 1, p->requiredCount, p->rounds + 1, p->attempt);
                break;
        }
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_MM_FILL, w->ladderAttempt + 1, w->detail);
        w->lastReportMs = genMs;
        w->lastStage = p->stage;
        w->lastAttempt = p->attempt;
    }

    // THE STOP (#950). A count of the rounds this attempt has run, never a
    // clock: `p->rounds` is a pure function of the frozen identity, so the same
    // seed reaches this line on every machine or on none. It only ever ends the
    // fill, with a status the ladder refuses to climb on. Checked BEFORE a round
    // runs, so a fill runs at most `roundBudget` rounds.
    if (w->roundBudget > 0u && (uint32_t)p->rounds >= w->roundBudget) {
        fprintf(stderr,
                "[SingleBag] per-attempt budget spent: %d of %u rounds run (%ums of generation on this host; a "
                "budget STOP, never a rung)\n",
                p->rounds, w->roundBudget, genMs);
        w->aborted = true;
        return 1;
    }
    return 0;
}

/** #1439: the Way of the Hero analysis's heartbeat (combo_woth.h), called before
 *  each of its rounds: a frame, and a caption at most once a second, so the
 *  overlay stays live through it. DISPLAY ONLY: the analysis is bounded by its own
 *  round cap, and nothing here can stop it or the creation. */
static void SingleBagWothProgress(void* ctx, int tested, int candidates) {
    SingleBagWatch* w = (SingleBagWatch*)ctx;
    const uint32_t genMs = SingleBagGenerationMs(w);
    if (ComboGenOverlay_WantsHeartbeat()) {
        (void)ComboGenOverlay_Heartbeat(genMs);
    }
    if (tested == 0 || genMs - w->lastReportMs >= 1000u) {
        snprintf(w->detail, sizeof(w->detail), "Finding the Way of the Hero (%d of %d)", tested + 1, candidates);
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_MM_FILL, w->ladderAttempt + 1, w->detail);
        w->lastReportMs = genMs;
    }
}

// ============================================================================
// The fill
// ============================================================================

/**
 * THE SHARED PAIR's families for this world (ABI 7, #976): every family the
 * FROZEN record arms whose two native copies are both registered and both
 * pooled (shared_resources.h). The registration is the one the runtime family bitset
 * gives from, so the bag row and the item the far game receives are the same
 * pair by construction. With every rule off (the shipped default) this is empty
 * and the compose is exactly the one before shared pairs existed.
 *
 * #1169 XW10: the SECOND family word's pairs (the shared warp songs) follow the
 * first word's, so a world with no second-word pair has the same pairs in the same
 * order (and its placement digest does not move). Writes at most @p cap pairs when
 * @p out is not NULL; returns how many pairs this world BUILDS, whatever @p cap.
 */
static int SingleBagBuildSharedPairs(ComboLogicSharedPair* out, int cap) {
    int n = 0;
    for (int word = 0; word < 2; ++word) {
        const uint32_t armed = word == 0 ? Combo_SharedFamiliesArmed() : Combo_SharedFamilies2Armed();
        for (uint32_t b = 0; b < RSBS_XITEM_BIT_COUNT; ++b) {
            const uint32_t bit = 1u << b;
            if ((armed & bit) == 0u) {
                continue;
            }
            uint16_t ootId = 0;
            uint16_t mmId = 0;
            const bool registered = word == 0 ? (Combo_SharedFamilyMemberId(GAME_OOT, bit, &ootId) &&
                                                 Combo_SharedFamilyMemberId(GAME_MM, bit, &mmId))
                                              : (Combo_SharedFamily2MemberId(GAME_OOT, bit, &ootId) &&
                                                 Combo_SharedFamily2MemberId(GAME_MM, bit, &mmId));
            if (!registered) {
                // A rule this build froze on with no item behind it on one side: the
                // pair cannot be built, and the row stays each game's own item. Loud,
                // because only a build that registers half a family can reach it.
                if (out != NULL) {
                    fprintf(stderr,
                            "[SingleBag] shared family %s is armed but not registered in both games; not paired\n",
                            word == 0 ? Combo_SharedFamilySlotName(bit) : Combo_SharedFamily2SlotName(bit));
                }
                continue;
            }
            const bool pooled =
                word == 0
                    ? (Combo_SharedFamilyMemberPooled(GAME_OOT, bit) && Combo_SharedFamilyMemberPooled(GAME_MM, bit))
                    : (Combo_SharedFamily2MemberPooled(GAME_OOT, bit) && Combo_SharedFamily2MemberPooled(GAME_MM, bit));
            if (!pooled) {
                // One game's own settings do not shuffle its copy (#1067: a game
                // without the ocarina-button shuffle). A shared row may be hosted in
                // either game, and that game could not hand the item on, so no pair:
                // each pooled copy stays its own game's item.
                continue;
            }
            if (out != NULL && n < cap) {
                out[n].ootId = ootId;
                out[n].mmId = mmId;
            }
            n++;
        }
    }
    return n;
}

/** The pairs into @p out, or -1 (loud, nothing written) when this world builds
 *  more pairs than @p cap holds: a silently dropped pair would leave its family's
 *  two copies unpaired, and every buffer is RSBS_XFAMILY_SLOT_COUNT, one per slot. */
static int SingleBagSharedPairs(ComboLogicSharedPair* out, int cap) {
    const int buildable = SingleBagBuildSharedPairs(NULL, 0);
    if (buildable > cap) {
        fprintf(stderr, "[SingleBag] shared pairs: %d buildable, buffer holds %d; refusing\n", buildable, cap);
        return -1;
    }
    return SingleBagBuildSharedPairs(out, cap);
}

int Combo_SingleBag_SharedPairs(ComboLogicSharedPair* out, int cap) {
    return (out == NULL || cap <= 0) ? 0 : SingleBagSharedPairs(out, cap);
}

static bool SingleBagOriginMayCross(uint8_t origin) {
    return Combo_ComboDirectionArms(origin) &&
           (Combo_ComboItemClassFor(origin) & (uint16_t)RSBS_ITEMCLASS_PROGRESSION) != 0u;
}

void Combo_SingleBag_CountHomeCapacity(const ComboLogicBagItem* bag, int bagCount, ComboSingleBagReport* r) {
    if (r == NULL) {
        return;
    }
    r->homeRequiredOoT = 0;
    r->homeRequiredMM = 0;
    for (int i = 0; bag != NULL && i < bagCount; ++i) {
        const uint16_t f = bag[i].bagFlags;
        if ((f & RSBS_COMBO_BAG_HOME_ONLY) == 0u || (f & RSBS_COMBO_BAG_SURPLUS) != 0u) {
            continue;
        }
        if (bag[i].item.originGame == (uint8_t)GAME_OOT) {
            r->homeRequiredOoT++;
        } else if (bag[i].item.originGame == (uint8_t)GAME_MM) {
            r->homeRequiredMM++;
        }
    }
    r->hostsOoT = Combo_Logic_LeftoverHosts(GAME_OOT, NULL, 0);
    r->hostsMM = Combo_Logic_LeftoverHosts(GAME_MM, NULL, 0);
    const GameId games[2] = { GAME_OOT, GAME_MM };
    for (int g = 0; g < 2; ++g) {
        const bool oot = games[g] == GAME_OOT;
        const int rows = oot ? r->homeRequiredOoT : r->homeRequiredMM;
        const int hosts = oot ? r->hostsOoT : r->hostsMM;
        const unsigned pieces = (unsigned)(oot ? r->triforcePiecesOoT : r->triforcePiecesMM);
        if (hosts < 0 || rows <= hosts) {
            continue;
        }
        // The Triforce Pieces clause and its remedy only when this game holds
        // pieces: a shortfall a direction made names only the remedies that apply.
        if (pieces > 0u) {
            fprintf(stderr,
                    "[SingleBag] home capacity: %d required item(s) that must stay in %s (%u of them Triforce "
                    "Pieces) for %d empty check(s) there, so no seed can place them; lower [Both Games] Triforce "
                    "Pieces, let that game's items cross, or shuffle more of its checks\n",
                    rows, Game_ToString(games[g]), pieces, hosts);
        } else {
            fprintf(stderr,
                    "[SingleBag] home capacity: %d required item(s) that must stay in %s for %d empty check(s) "
                    "there, so no seed can place them; let that game's items cross, or shuffle more of its checks\n",
                    rows, Game_ToString(games[g]), hosts);
        }
    }
}

/**
 * #1169 XW15, THE FIXED SHARED SONGS: under Shuffle Songs Off each armed SHARED WARP
 * SONG stays on its own default check (Combo_SharedWarpSongFixedAtHome), placed by
 * its own game and in no table, so the coordinator is told to credit the other game
 * when that check is reached (Combo_Logic_SetFixedCredits). One credit per family:
 * an OoT host hands MM its member, an MM host hands OoT its member. A family with a
 * host in both games would be two copies of one item, so it is refused
 * (ERR_BAD_REQUEST); with no host in either (a started song OoT did not place, a
 * mode other than Off) it adds nothing. Returns RSBS_COMBO_LOGIC_OK otherwise. Every
 * world without a shared warp song frozen on sets no credit.
 */
static int SingleBagSetFixedSharedSongCredits(void) {
    ComboLogicFixedCredit credits[RSBS_COMBO_LOGIC_FIXED_CREDIT_CAP];
    int count = 0;
    const uint32_t shared = Combo_SharedFamilies2Armed() & RSBS_XITEM2_WARP_SONG_SHARED;
    for (uint32_t b = 0; shared != 0u && b < RSBS_XITEM2_BIT_COUNT; ++b) {
        const uint32_t bit = 1u << b;
        uint16_t ootId = 0;
        uint16_t mmId = 0;
        if ((shared & bit) == 0u || !Combo_SharedFamily2MemberId(GAME_OOT, bit, &ootId) ||
            !Combo_SharedFamily2MemberId(GAME_MM, bit, &mmId)) {
            continue;
        }
        const int ootHost = OoT_ComboLogic_FixedSharedSongHost(ootId);
        const int mmHost = MM_ComboLogic_FixedSharedSongHost(mmId);
        if (ootHost >= 0 && mmHost >= 0) {
            fprintf(stderr, "[SingleBag] shared family %s sits on a fixed check in both games (OoT %d, MM %d)\n",
                    Combo_SharedFamily2SlotName(bit), ootHost, mmHost);
            return RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        }
        if (ootHost < 0 && mmHost < 0) {
            continue;
        }
        if (count >= RSBS_COMBO_LOGIC_FIXED_CREDIT_CAP) {
            return RSBS_COMBO_LOGIC_ERR_CAPACITY;
        }
        ComboLogicFixedCredit* c = &credits[count++];
        memset(c, 0, sizeof(*c));
        c->hostGame = (uint8_t)(ootHost >= 0 ? GAME_OOT : GAME_MM);
        c->hostCheck = (uint16_t)(ootHost >= 0 ? ootHost : mmHost);
        c->farItem.originGame = (uint8_t)(ootHost >= 0 ? GAME_MM : GAME_OOT);
        c->farItem.id = ootHost >= 0 ? mmId : ootId;
        fprintf(stderr, "[SingleBag] shared family %s stays on its own %s check %u; %s's logic is credited there\n",
                Combo_SharedFamily2SlotName(bit), ootHost >= 0 ? "OoT" : "MM", (unsigned)c->hostCheck,
                ootHost >= 0 ? "MM" : "OoT");
    }
    return Combo_Logic_SetFixedCredits(count > 0 ? credits : NULL, count) < 0 ? RSBS_COMBO_LOGIC_ERR_BAD_REQUEST
                                                                              : RSBS_COMBO_LOGIC_OK;
}

int Combo_SingleBag_Run(const uint16_t* mmItems, const uint16_t* mmFlags, const uint32_t* mmConfine, int mmCount,
                        int ladderAttempt, uint32_t roundBudget, uint8_t* outMmInBag, ComboSingleBagReport* out) {
    const uint32_t t0 = Combo_GenBudget_NowMs();
    memset(&sReport, 0, sizeof(sReport));
    sReport.ladderAttempt = ladderAttempt;
    sReport.roundBudget = roundBudget;
    sReport.goal = gComboCtx.comboSettings.goal;
    sReport.rung = gComboCtx.comboSettings.logicRung;
    sReport.hostsOoT = -1;
    sReport.hostsMM = -1;
    int status = RSBS_COMBO_LOGIC_OK;
    bool fillRan = false; // #1483: Combo_Logic_RunFill was called (the home-capacity counts need it)
    // #1439: an attempt's Way of the Hero is its own proved world's or none, so a
    // failed attempt never leaves an earlier one's behind.
    Combo_Woth_Clear();
    Combo_WothRecord_Clear(&gComboCtx.comboWoth);
    // #1169 XW15: a fixed shared credit is a fact of the world this attempt fills,
    // so none survives from an earlier one.
    (void)Combo_Logic_SetFixedCredits(NULL, 0);

    if (mmCount < 0 || (mmCount > 0 && (mmItems == NULL || mmFlags == NULL))) {
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }
    if (!Combo_ComboSettingsFrozen() || !Combo_ForeignPairingActive()) {
        // The fill's every input is a frozen fact; with no frozen record there is
        // no GOAL, rung, direction or class bitset to fill under.
        fprintf(stderr, "[SingleBag] refused: no frozen paired identity (frozen=%d paired=%d)\n",
                Combo_ComboSettingsFrozen() ? 1 : 0, Combo_ForeignPairingActive() ? 1 : 0);
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }
    if (!OoT_ComboLogic_GeneralPassDeferred()) {
        // OoT's world is not waiting at its general pass: either it was never
        // generated in this process, or it already finished. Filling now would
        // place the bag over a world that already has a general pass.
        fprintf(stderr, "[SingleBag] refused: OoT's world is not at its general-pass point\n");
        status = RSBS_COMBO_LOGIC_ERR_BAD_REQUEST;
        goto finish;
    }

    // --- the rows: OoT's general-pass pool, then MM's pool -----------------
    {
        const int ootTotal = OoT_ComboLogic_ExportPool(0, sOoTItems, NULL, sOoTFlags, COMBO_SINGLE_BAG_ROW_CAP);
        if (ootTotal < 0) {
            fprintf(stderr, "[SingleBag] refused: OoT's pool export is not ready\n");
            status = RSBS_COMBO_LOGIC_ERR_NO_ENGINE;
            goto finish;
        }
        if (ootTotal + mmCount > COMBO_SINGLE_BAG_ROW_CAP) {
            fprintf(stderr, "[SingleBag] refused: %d + %d pool rows exceed the %d-row buffer\n", ootTotal, mmCount,
                    COMBO_SINGLE_BAG_ROW_CAP);
            status = RSBS_COMBO_LOGIC_ERR_CAPACITY;
            goto finish;
        }
        sReport.ootRows = ootTotal;
        sReport.mmRows = mmCount;
        // #1542: OoT's songs carry the shared song pool's wide group while it is
        // armed (0 for every row otherwise, so no other world moves).
        memset(sOoTConfine, 0, sizeof(sOoTConfine[0]) * (size_t)ootTotal);
        (void)OoT_ComboLogic_PoolRowConfineMasks(sOoTItems, ootTotal, sOoTConfine);
        for (int i = 0; i < ootTotal; ++i) {
            memset(&sRows[i], 0, sizeof(sRows[i]));
            sRows[i].item.originGame = (uint8_t)GAME_OOT;
            sRows[i].item.id = sOoTItems[i];
            sRows[i].poolFlags = sOoTFlags[i];
        }
        for (int j = 0; j < mmCount; ++j) {
            ComboLogicPoolRow* r = &sRows[ootTotal + j];
            memset(r, 0, sizeof(*r));
            r->item.originGame = (uint8_t)GAME_MM;
            r->item.id = mmItems[j];
            r->poolFlags = mmFlags[j];
        }
    }

    // --- 1. the bag -----------------------------------------------------------
    {
        Combo_GenProgress_Report((uint8_t)RSBS_GENPHASE_MM_FILL, ladderAttempt + 1,
                                 "Building the single bag of both worlds' items");
        ComboLogicComposeRequest creq;
        memset(&creq, 0, sizeof(creq));
        creq.rows = sRows;
        creq.rowCount = sReport.ootRows + sReport.mmRows;
        creq.armedOoT = OoT_ComboLogic_ConfinementArmed() | Combo_ItemClassArmedFromFrozen((uint8_t)GAME_OOT);
        creq.armedMM = Combo_ItemClassArmedFromFrozen((uint8_t)GAME_MM);
        creq.composeFlags = RSBS_COMBO_COMPOSE_ADMIT_CONFINED_HOME;
        // THE SHARED-QUANTITY TRIM (combo_logic.h): ON, never KEEP_ALL. Both
        // starting bars come from the frozen settings; the composer refuses a
        // zero rather than guessing one.
        creq.trimSeed = Combo_SingleBag_TrimSeed();
        creq.startingHealthOoT = OoT_ComboLogic_StartingHealth();
        creq.startingHealthMM = MM_ComboLogic_StartingHealth();
        creq.quantityFlags = 0u;
        // THE SHARED PAIR (ABI 7, #976): empty unless a frozen rule arms one.
        static ComboLogicSharedPair sPairs[RSBS_XFAMILY_SLOT_COUNT];
        const int pairCount = SingleBagSharedPairs(sPairs, (int)RSBS_XFAMILY_SLOT_COUNT);
        if (pairCount < 0) {
            // #1169 XW10: more pairs than the buffer holds (SingleBagSharedPairs said
            // so); never compose with a family silently unpaired.
            status = RSBS_COMBO_LOGIC_ERR_CAPACITY;
            fprintf(stderr, "[SingleBag] the bag could not be composed: %s (shared pairs)\n",
                    Combo_Logic_StatusName(status));
            goto finish;
        }
        creq.sharedPairCount = pairCount;
        creq.sharedPairs = (creq.sharedPairCount > 0) ? sPairs : NULL;
        // #1169 XW15: the shared warp songs that stay on their own check under Off
        // make no bag row (neither pool holds a copy); the coordinator credits the
        // other game where each one sits. Set before the fill, kept for its WotH
        // rounds and every exit round until Combo_SingleBag_Forget.
        status = SingleBagSetFixedSharedSongCredits();
        if (status != RSBS_COMBO_LOGIC_OK) {
            fprintf(stderr, "[SingleBag] the bag could not be composed: %s (fixed shared songs)\n",
                    Combo_Logic_StatusName(status));
            goto finish;
        }
        sReport.trimSeed = creq.trimSeed;
        sReport.startingHealthOoT = creq.startingHealthOoT;
        sReport.startingHealthMM = creq.startingHealthMM;
        status = Combo_Logic_ComposeBag(&creq, sBag, RSBS_COMBO_LOGIC_BAG_CAP, sBagPoolIndex, &sReport.compose);
        if (status != RSBS_COMBO_LOGIC_OK) {
            fprintf(stderr, "[SingleBag] the bag could not be composed: %s\n", Combo_Logic_StatusName(status));
            goto finish;
        }
        sReport.bagCount = sReport.compose.bagCount;
        sTrimmedCount = 0;
        int poolRow = -1;
        for (int i = 0; Combo_Logic_ComposeTrimmedAt(i, &poolRow); ++i) {
            // The composer refuses (CAPACITY) a compose whose trimmed record
            // overflows, so every trimmed row fits here.
            sTrimmedPoolRows[sTrimmedCount++] = poolRow;
            if (poolRow < sReport.ootRows) {
                sReport.trimmedOoT++;
            } else {
                sReport.trimmedMM++;
            }
        }
    }

    // --- 2. which origins may cross -------------------------------------------
    {
        const bool ootMayCross = SingleBagOriginMayCross((uint8_t)GAME_OOT);
        const bool mmMayCross = SingleBagOriginMayCross((uint8_t)GAME_MM);
        for (int i = 0; i < sReport.bagCount; ++i) {
            const bool mayCross = (sBag[i].item.originGame == (uint8_t)GAME_OOT) ? ootMayCross : mmMayCross;
            // A shared pair never crosses (each side hosts its own half), so the
            // direction does not confine it, and HOME_ONLY would refuse the fill.
            const bool sharedPair = (sBag[i].bagFlags & RSBS_COMBO_BAG_SHARED_PAIR) != 0u;
            if (!mayCross && !sharedPair) {
                sBag[i].bagFlags |= RSBS_COMBO_BAG_HOME_ONLY;
            }
            // THE HOST GROUPS (#1502): a bag row taken from a confined pool row
            // keeps its mask. A LOCAL mask (MM's dungeons, MM's own Song Locations)
            // keeps the row home as well. A shared pair is exempt from a local mask:
            // it has no single home, so a confined song a frozen shared-song rule
            // pairs roams, as OoT's held-back copy does (#1085).
            // THE COMBO-WIDE GROUPS (#1542, combo_logic.h ABI 11): a WIDE mask (the
            // shared song pool's) confines without keeping the row home, and a
            // shared pair keeps it: the pair's one copy sits on a song check of
            // either game (ruling D5). The pair's kept row is its earlier pool
            // position, OoT's (the rows above are OoT's first), and both halves of
            // a shared song carry the same wide mask under the pool.
            sBagConfine[i] = 0u;
            const int poolRow = sBagPoolIndex[i];
            uint32_t mask = 0u;
            if (poolRow >= 0 && poolRow < sReport.ootRows && sBag[i].item.originGame == (uint8_t)GAME_OOT) {
                mask = sOoTConfine[poolRow];
            } else if (mmConfine != NULL && poolRow >= sReport.ootRows && poolRow < sReport.ootRows + sReport.mmRows &&
                       sBag[i].item.originGame == (uint8_t)GAME_MM) {
                mask = mmConfine[poolRow - sReport.ootRows];
            }
            const bool wideOnly = mask != 0u && (mask & ~RSBS_COMBO_GROUP_WIDE_MASK) == 0u;
            if (mask != 0u && wideOnly) {
                sBagConfine[i] = mask;
                sReport.confinedRows++;
            } else if (mask != 0u && !sharedPair) {
                sBagConfine[i] = mask;
                sBag[i].bagFlags |= RSBS_COMBO_BAG_HOME_ONLY;
                sReport.confinedRows++;
            } else if (mask != 0u) {
                fprintf(stderr,
                        "[SingleBag] %s item %u is confined (mask 0x%08X) but is a shared pair's row; it roams\n",
                        Game_ToString((GameId)sBag[i].item.originGame), (unsigned)sBag[i].item.id, (unsigned)mask);
            }
            if ((sBag[i].bagFlags & RSBS_COMBO_BAG_HOME_ONLY) != 0u) {
                sReport.homeOnlyRows++;
            }
            // The ADR 0011 selection bit the row was admitted under, carried into
            // the placement tables and the crossing store (combo_logic.h:
            // "carried, not filtered on"). Every bag row is O8-progression.
            sBag[i].itemClass = (uint16_t)RSBS_ITEMCLASS_PROGRESSION;
        }
        fprintf(
            stderr,
            "[SingleBag] bag: %d rows (OoT %d req + %d surplus + %d confined; MM %d req + %d surplus + %d confined) "
            "from %d OoT + %d MM pool rows; %d home-only; crossings armed OoT->MM=%d MM->OoT=%d; filler left to "
            "each game: OoT %d junk / %d renewable / %d trap / %d trimmed, MM %d junk / %d renewable / %d trap / "
            "%d trimmed (trim seed %08X, starting health OoT 0x%X MM 0x%X); %d confined to a group\n",
            sReport.bagCount, sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_REQUIRED],
            sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_SURPLUS],
            sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_CONFINED],
            sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_REQUIRED],
            sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_SURPLUS],
            sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_CONFINED], sReport.ootRows, sReport.mmRows,
            sReport.homeOnlyRows, ootMayCross ? 1 : 0, mmMayCross ? 1 : 0,
            sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_JUNK],
            sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_RENEWABLE],
            sReport.compose.perGame[GAME_OOT].rows[RSBS_COMBO_COMPOSE_TRAP], sReport.trimmedOoT,
            sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_JUNK],
            sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_RENEWABLE],
            sReport.compose.perGame[GAME_MM].rows[RSBS_COMBO_COMPOSE_TRAP], sReport.trimmedMM, sReport.trimSeed,
            (unsigned)sReport.startingHealthOoT, (unsigned)sReport.startingHealthMM, sReport.confinedRows);
    }

    // --- 3 + 4. the fill, seeded from the identity, stopped by the budget -----
    {
        SingleBagWatch watch;
        memset(&watch, 0, sizeof(watch));
        watch.ladderAttempt = ladderAttempt;
        watch.roundBudget = roundBudget;
        watch.startMs = t0;
        watch.presentStartMs = Combo_GenProgress_PresentationMs();
        watch.lastStage = -1;
        watch.lastAttempt = -1;

        sReport.seed = Combo_SingleBag_SeedFor(ladderAttempt);
        ComboLogicFillRequest freq;
        memset(&freq, 0, sizeof(freq));
        freq.bag = sBag;
        freq.bagCount = sReport.bagCount;
        freq.goal = sReport.goal;
        freq.logicRung = sReport.rung;
        // #1389: the frozen combo requirement the triforce-hunt GOAL is proved
        // against (0 under every other goal, which the fill ignores). Both
        // games' pieces are already bag rows: their WORLD_EVENT family is armed
        // by no record, so the compose admits them HOME_ONLY (each game's
        // pieces on its own hosts), and the proof counts them.
        freq.triforceRequired = Combo_TriforceHuntRequired();
        // #1483: each game's share of the pieces, read now, for the refusal's
        // words: a failed creation retracts the triforce record before
        // Generate's toast is raised.
        if (Combo_TriforceHuntArmed()) {
            sReport.triforcePiecesOoT = gComboCtx.comboTriforce.totalOoT;
            sReport.triforcePiecesMM = gComboCtx.comboTriforce.totalMM;
        }
        freq.seed = sReport.seed;
        freq.maxAttempts = 0; // RSBS_COMBO_LOGIC_FILL_RETRIES batch roll-backs
        // Every crossing is delivered through one shared-item slot; bound each
        // side by the outbox's logical capacity, so no pickup can find it full
        // (context.h). It equals a side's placement cap, so it never binds.
        freq.maxCrossingsPerSide = (uint16_t)RSBS_CROSSINGS_PER_SIDE_MAX;
        freq.observer = SingleBagObserve;
        freq.observerCtx = &watch;
        // OoTMM's critical-renewables rule (critical_renewables.h): the fill
        // applies it under All Locations only, over both games' registered sources.
        freq.criticalRenewables = 1u;
        // THE HOST GROUPS (#1502): only when a row is confined, so a world with
        // every placement option at its default runs the mask-free fill.
        freq.confineMasks = (sReport.confinedRows > 0) ? sBagConfine : NULL;
        // THE SHARED SONG POOL (#1542, ruling D1; combo_logic.h ABI 11): under it
        // the song checks of both games take songs only (OoTMM placeSemiShuffled
        // then fillJunk), and the checks the songs leave over stay empty for each
        // game's own junk step. 0 in every other world.
        freq.exclusiveGroups = OoT_ComboLogic_SongPoolUnion() ? RSBS_COMBO_GROUP_SONG_LOCATIONS : 0u;
        sReport.exclusiveGroups = freq.exclusiveGroups;
        status = Combo_Logic_RunFill(&freq, &sReport.fill);
        fillRan = true;
        if (status != RSBS_COMBO_LOGIC_OK) {
            fprintf(stderr, "[SingleBag] fill failed on ladder attempt %d: %s after %d batch attempt(s), %d rounds%s\n",
                    ladderAttempt + 1, Combo_Logic_StatusName(status), sReport.fill.attempts, sReport.fill.rounds,
                    watch.aborted ? " (the per-attempt round budget stopped it)" : "");
            if (status == RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE) {
                // The All Locations pre-flight (ABI 8): a fact of the frozen settings,
                // so the creation refuses with the names rather than re-seeding.
                char names[512];
                const int n = Combo_Logic_FormatUnreachable(names, sizeof(names), 8);
                fprintf(stderr,
                        "[SingleBag] All Locations: %d location(s) no seed can reach under these settings: %s\n", n,
                        names);
            } else if (status == RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY) {
                ComboLogicGroupShortfall shortfall;
                if (Combo_Logic_LastGroupShortfall(&shortfall)) {
                    sReport.shortGroupMask = shortfall.mask;
                }
                if (Combo_Logic_LastGroupShortfall(&shortfall) && (shortfall.mask & RSBS_COMBO_GROUP_WIDE_MASK) != 0u) {
                    fprintf(stderr,
                            "[SingleBag] the song checks hold fewer checks than songs: %d song(s) (%d Ocarina of "
                            "Time, %d Majora's Mask, the rest shared) for %d song check(s) they may use (%s)\n",
                            shortfall.rows, shortfall.rowsOoT, shortfall.rowsMM, shortfall.hosts,
                            shortfall.game == (uint8_t)GAME_NONE ? "both games"
                                                                 : Game_ToString((GameId)shortfall.game));
                } else if (Combo_Logic_LastGroupShortfall(&shortfall)) {
                    fprintf(stderr,
                            "[SingleBag] a confinement group holds more items than checks: %d %s item(s) for %d "
                            "check(s) (group mask 0x%08X)\n",
                            shortfall.rows, Game_ToString((GameId)shortfall.game), shortfall.hosts,
                            (unsigned)shortfall.mask);
                }
            } else if (status == RSBS_COMBO_LOGIC_ERR_GOAL_UNREACHABLE) {
                fprintf(stderr,
                        "[SingleBag] All Locations: no seed can finish the goal under these settings (OoT half %d, MM "
                        "half %d with every required item assumed)\n",
                        sReport.fill.goalOoT, sReport.fill.goalMM);
            }
            goto finish;
        }
        // TEST ONLY: H4's measurement point (#575), with both engines live over
        // the proven world. NULL in production.
        if (sPostFillProbe != NULL) {
            sPostFillProbe(sPostFillProbeCtx);
        }
        // #1439: the pair's Way of the Hero, over the proved world while both
        // engines are still live (combo_woth.h): #575 H4's required set, bounded
        // by its own round cap. Its rounds move no placement and are not charged
        // to the fill's budget; it writes gComboCtx.comboWoth for MM's stones and
        // leaves the set resident for OoT's remainder.
        (void)Combo_Woth_RunAtCreation(sReport.goal, freq.triforceRequired, SingleBagWothProgress, &watch);
    }

    // --- success: the bookkeeping each game's own pass needs -----------------
    {
        int ootBag = 0;
        int ootTrimmed = 0;
        if (outMmInBag != NULL) {
            memset(outMmInBag, RSBS_SINGLE_BAG_MM_ROW_OWN_PASS, (size_t)mmCount);
        }
        for (int i = 0; i < sReport.bagCount; ++i) {
            const int poolRow = sBagPoolIndex[i];
            if (poolRow < sReport.ootRows) {
                sOoTBagRows[ootBag++] = poolRow;
            } else if (outMmInBag != NULL) {
                outMmInBag[poolRow - sReport.ootRows] = RSBS_SINGLE_BAG_MM_ROW_IN_BAG;
            }
        }
        // THE TRIMMED ROWS go back to their ORIGIN game's own pass as filler,
        // never as the item (combo_logic.h, THE SHARED-QUANTITY TRIM, rule 5).
        for (int i = 0; i < sTrimmedCount; ++i) {
            const int poolRow = sTrimmedPoolRows[i];
            if (poolRow < sReport.ootRows) {
                sOoTTrimmedRows[ootTrimmed++] = poolRow;
            } else if (outMmInBag != NULL) {
                outMmInBag[poolRow - sReport.ootRows] = RSBS_SINGLE_BAG_MM_ROW_TRIMMED;
            }
        }
        OoT_ComboLogic_NoteBagRows(sOoTBagRows, ootBag);
        OoT_ComboLogic_NoteTrimmedRows(sOoTTrimmedRows, ootTrimmed);

        ComboLogicPlacement p;
        for (int i = 0; i < Combo_Logic_PlacementCount(GAME_MM); ++i) {
            if (Combo_Logic_PlacementAt(GAME_MM, i, &p) && p.item.originGame == (uint8_t)GAME_OOT) {
                sReport.crossingsIntoMM++;
            }
        }
        for (int i = 0; i < Combo_Logic_PlacementCount(GAME_OOT); ++i) {
            if (Combo_Logic_PlacementAt(GAME_OOT, i, &p) && p.item.originGame == (uint8_t)GAME_MM) {
                sReport.crossingsIntoOoT++;
            }
        }
    }

finish:
    if (status != RSBS_COMBO_LOGIC_OK && sReport.fill.attempts > 0) {
        // Roll BOTH engines back to their state on entry: OoT's general-pass hosts
        // to RG_NONE, MM's checks to their priors. The MM ladder restores MM's
        // whole save per attempt anyway, but nothing restores OoT's world except
        // this, and the next attempt must see its general pass empty.
        Combo_Logic_ResetPlacements();
    }
    if (Combo_SingleBag_FailureCountsHomeCapacity(fillRan, status, sReport.fill.attempts)) {
        // #1483: after the roll-back (or an early pre-attempt capacity refusal),
        // so each game's leftover hosts are all of its empty hosts.
        Combo_SingleBag_CountHomeCapacity(sBag, sReport.bagCount, &sReport);
    }
    sReport.status = status;
    // THE TOTAL'S CHARGE (#950): every round this fill ran, whatever its status,
    // so the ladder's between-attempt check sees dead ends and successes alike.
    if (sReport.fill.rounds > 0) {
        Combo_GenBudget_ChargeRounds((uint32_t)sReport.fill.rounds);
    }
    sReport.wallMs = Combo_GenBudget_NowMs() - t0;
    // Mirrored into the session log (#1286): the round count a playtest report
    // needs to say whether a slow creation was near its budget.
    Combo_GenProgress_LogLine(
        "[SingleBag] ladder attempt %d: %s — goal %u rung %u seed %08X; %d bag rows (%d OoT + %d MM trimmed to "
        "filler); %d rows placed (%d required, %d surplus, %d dropped), %d crossings into MM, %d into OoT, "
        "leftovers OoT %d / MM %d; %d batch attempt(s), %d rounds (budget %u), %ums",
        ladderAttempt + 1, Combo_Logic_StatusName(status), (unsigned)sReport.goal, (unsigned)sReport.rung, sReport.seed,
        sReport.bagCount, sReport.trimmedOoT, sReport.trimmedMM, sReport.fill.placed, sReport.fill.requiredPlaced,
        sReport.fill.surplusPlaced, sReport.fill.surplusDropped, sReport.crossingsIntoMM, sReport.crossingsIntoOoT,
        sReport.fill.leftoverHostsOoT, sReport.fill.leftoverHostsMM, sReport.fill.attempts, sReport.fill.rounds,
        roundBudget, sReport.wallMs);
    if (out != NULL) {
        *out = sReport;
    }
    return status;
}

bool Combo_SingleBag_FailureCountsHomeCapacity(bool fillRan, int status, int attempts) {
    return fillRan && status != RSBS_COMBO_LOGIC_OK && (attempts > 0 || status == RSBS_COMBO_LOGIC_ERR_HOME_CAPACITY);
}

bool Combo_SingleBag_StatusIsWorldDeadEnd(int status, uint8_t rung) {
    switch (status) {
        // Under a PROVING rung a dead end is a fact of the order this seed drew. Under
        // `none` the fill draws from EVERY empty host and runs one attempt, so running
        // out is a capacity fact about the two pools (combo_logic.h on
        // ERR_NO_CANDIDATE): no seed has more hosts, and climbing would only repeat it.
        case RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE:
            return rung != (uint8_t)RSBS_COMBO_RUNG_NONE;
        case RSBS_COMBO_LOGIC_ERR_GOAL_UNPROVABLE:
            return true;
        // Never a rung (ABI 8): after the All Locations pre-flight held, a final round
        // that misses a location is an engine fault, not a seed's; and the pre-flight's
        // own refusals (a location, or the GOAL, no seed reaches) are facts of the
        // settings, fixed at Generate.
        case RSBS_COMBO_LOGIC_ERR_NOT_ALL_REACHED:
        case RSBS_COMBO_LOGIC_ERR_LOCATIONS_UNREACHABLE:
        case RSBS_COMBO_LOGIC_ERR_GOAL_UNREACHABLE:
        case RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY:
        case RSBS_COMBO_LOGIC_ERR_HOME_CAPACITY:
        default:
            return false;
    }
}

uint32_t Combo_SingleBag_HomeShortGames(const ComboSingleBagReport* r) {
    if (r == NULL || r->status == RSBS_COMBO_LOGIC_OK) {
        return 0u;
    }
    uint32_t bits = 0u;
    if (r->hostsOoT >= 0 && r->homeRequiredOoT > r->hostsOoT) {
        bits |= RSBS_COMBO_HALF_OOT;
    }
    if (r->hostsMM >= 0 && r->homeRequiredMM > r->hostsMM) {
        bits |= RSBS_COMBO_HALF_MM;
    }
    return bits;
}

bool Combo_SingleBag_SongCapacityShort(const ComboSingleBagReport* r) {
    return r != NULL && r->status == RSBS_COMBO_LOGIC_ERR_GROUP_CAPACITY &&
           (r->shortGroupMask & RSBS_COMBO_GROUP_WIDE_MASK) != 0u;
}

void Combo_SingleBag_ClearLastReport(void) {
    memset(&sReport, 0, sizeof(sReport));
}

void Combo_SingleBag_TestSetLastReport(const ComboSingleBagReport* r) {
    if (r == NULL) {
        memset(&sReport, 0, sizeof(sReport));
        return;
    }
    sReport = *r;
}

void Combo_SingleBag_Forget(void) {
    // Tables first (no engine call), then each engine's own record.
    (void)Combo_Logic_HydrateTables(NULL, 0, NULL, 0);
    // #1169 XW15: the fixed shared credits are the forgotten world's, too.
    (void)Combo_Logic_SetFixedCredits(NULL, 0);
    OoT_ComboLogic_ForgetPlacements();
    MM_ComboLogic_ForgetPlacements();
}
