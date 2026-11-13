/**
 * @file shared_resources.c
 * @brief Shared cross-game resource merge core (#525).
 *
 * See shared_resources.h for the model, the two merge disciplines and why the
 * watermark is mandatory rather than an optimization.
 *
 * This TU is intentionally free of game headers — it touches
 * gComboCtx.sharedResources and a small RAM-only watermark table and nothing
 * else — so it compiles into the shared redship_common library and both games'
 * per-game shims call it directly with values they read out of their own
 * gSaveContext. The unit conversions and field names stay on the game side,
 * where the headers are; the merge rules stay here, where there is exactly one
 * copy of them.
 */

#include "shared_resources.h"
// #668 armed the ocarina kind from the frozen combo record's comboFlags; #879
// retired that rule and the kind is never armed now. foreign_items.h is
// game-header-free too, so this TU stays compilable in the ROM-free harness.
#include "foreign_items.h"
#include "triforce_hunt.h"   // Combo_TriforceHuntArmed: the O10 kind's arming gate
#include "shared_items.h"    // #1061: a pack's kind and size, from the item class owner
#include "warp_song_items.h" // #1169 XW14: Combo_SharedFamily2DisplayName is declared there
#include "warp_songs_mm.h"   // #1169 XW14: Combo_MMWarpSongOoTName, a shared song's plain name
#include <stdio.h>
#include <string.h>

// ============================================================================
// The RAM-only watermark table: how much of the shared pool is currently
// MATERIALIZED in the live save, per resource.
//
// Indexed by KIND, not by slot: slot positions are found by scan and are not
// promised to be stable, while a kind is a fixed enumerator. `owner` is the
// game the watermark describes — GAME_NONE means "no game has been applied to
// for this resource in this process", which is the first-harvest seed case the
// header spells out.
// ============================================================================

typedef struct {
    uint16_t value; // the live value at the last apply/harvest sync point
    uint8_t owner;  // GameId the watermark belongs to; GAME_NONE = not live
} SharedResWatermark;

static SharedResWatermark sWatermarks[RSBS_SHARED_RES_KIND_COUNT];

static bool IsRealGame(GameId game) {
    return game == GAME_OOT || game == GAME_MM;
}

static bool IsRealKind(uint8_t kind) {
    return kind != RSBS_SHARED_RES_NONE && kind < RSBS_SHARED_RES_KIND_COUNT;
}

/**
 * Which discipline `kind` merges under. This switch is the ONE authoritative
 * statement of the split — the RSBS_SHARED_RES_F_MONOTONIC flag byte stored on
 * the slot is descriptive (so a reader or a save inspector can tell them apart
 * without this table), never the thing decided on.
 */
static bool IsMonotonicKind(uint8_t kind) {
    switch (kind) {
        case RSBS_SHARED_RES_WALLET_TIER:
        case RSBS_SHARED_RES_HEALTH_QUARTERS:
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
        case RSBS_SHARED_RES_MAGIC_LEVEL:
        case RSBS_SHARED_RES_QUIVER_TIER:
        case RSBS_SHARED_RES_BOMB_BAG_TIER:
        case RSBS_SHARED_RES_STICK_TIER:
        case RSBS_SHARED_RES_NUT_TIER:
        case RSBS_SHARED_RES_HOOKSHOT_TIER:
        case RSBS_SHARED_RES_OCARINA_TIER:
        // ADR 0010 O10: the one triforce piece count. MONOTONIC because a piece
        // is never spent. A consumable harvest reads a LOWER live value after a
        // full apply (a counter reset, a stale save) as pieces spent and takes
        // them out of the pool; max-merge keeps the count. That is the only
        // behaviour that tells the two disciplines apart: a consumable apply
        // records what it materialized as the watermark, so ordinary collects
        // across switches sum the same way under either (the cross-game sum in
        // combo-triforce-hunt passes under both; its discipline-pin leg is the
        // one that fails under CONSUMABLE).
        case RSBS_SHARED_RES_TRIFORCE_PIECES:
        // #886: the Kokiri Sword, 0/1. A sword lost in MM (Takkuri) must not take
        // OoT's away: max-merge keeps the 1.
        case RSBS_SHARED_RES_KOKIRI_SWORD:
        // #970: the bottle count. Bottles are never lost, but a rewound or stale
        // save can harvest fewer than the pair holds; max-merge keeps the count.
        case RSBS_SHARED_RES_BOTTLE_COUNT:
            return true;
        default:
            // The spent quantities — rupees, current health, current magic, and
            // the five ammo counts — are delta-harvested.
            return false;
    }
}

// ============================================================================
// TWO PHYSICAL BLOCKS, ONE LOGICAL ARRAY.
//
// gComboCtx.sharedResources[8] and gComboCtx.sharedResourcesExt[12] are
// separate fields at separate .redsave offsets — they must be, because
// widening the first in place would move the second (and everything carved
// after it) off the offset every shipped save stored it at. But nothing above
// this line should have to know that: a "slot" here is a LOGICAL index into
// the concatenation, and every scan below covers the whole range in one pass.
//
// This resolver is the only place the split is visible. Keeping it that way is
// the point — a scan that stops at the first block's end would split a kind
// across blocks (two slots claiming one resource) or report "full" with twelve
// free slots behind it.
// ============================================================================

static ComboSharedResource* SlotAt(int logical) {
    if (logical < 0 || logical >= (int)RSBS_SHARED_RESOURCE_TOTAL_CAP) {
        return NULL;
    }
    if (logical < (int)RSBS_SHARED_RESOURCE_CAP) {
        return &gComboCtx.sharedResources[logical];
    }
    return &gComboCtx.sharedResourcesExt[logical - (int)RSBS_SHARED_RESOURCE_CAP];
}

// Locate the occupied slot holding `kind`, or -1 when the resource has never
// been shared. Slots are unordered; there is at most one per kind because
// FindOrCreateSlot always looks first — across BOTH blocks.
static int FindSlot(uint8_t kind) {
    for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_TOTAL_CAP; i++) {
        if (SlotAt(i)->kind == kind) {
            return i;
        }
    }
    return -1;
}

// Locate `kind`'s slot, claiming a free one (value 0) if it has none. Returns
// -1 only when every slot is taken by some OTHER resource. Nothing is ever
// evicted: unlike a shared ITEM, a shared resource has no "redeemed" state, so
// there is no slot that is safe to reclaim — a full array means the resource
// set outgrew both blocks, which is a carve-time bug, not a runtime condition.
// The blocks are sized with room for the whole class.
static int FindOrCreateSlot(uint8_t kind) {
    int slot = FindSlot(kind);
    if (slot >= 0) {
        return slot;
    }
    for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_TOTAL_CAP; i++) {
        ComboSharedResource* entry = SlotAt(i);
        if (entry->kind == RSBS_SHARED_RES_NONE) {
            entry->kind = kind;
            entry->flags = IsMonotonicKind(kind) ? RSBS_SHARED_RES_F_MONOTONIC : 0u;
            entry->value = 0u;
            return i;
        }
    }
    fprintf(stderr,
            "[combo] shared-resource slots full across both blocks; dropping kind=%u (carve a THIRD "
            "block from reserved[] and span it too — never widen an existing block in place)\n",
            (unsigned)kind);
    return -1;
}

// ============================================================================
// The canonical heart quantity. See the header for why hearts are ONE number.
// ============================================================================

uint16_t Combo_MakeHealthQuarters(uint16_t capacity, uint16_t pieces) {
    uint32_t quarters = (uint32_t)capacity + (uint32_t)pieces * 4u;
    if (quarters > RSBS_SHARED_RES_MAX_HEALTH_QUARTERS) {
        quarters = RSBS_SHARED_RES_MAX_HEALTH_QUARTERS;
    }
    return (uint16_t)quarters;
}

void Combo_SplitHealthQuarters(uint16_t quarters, uint16_t* outCapacity, uint16_t* outPieces) {
    if (quarters > RSBS_SHARED_RES_MAX_HEALTH_QUARTERS) {
        quarters = (uint16_t)RSBS_SHARED_RES_MAX_HEALTH_QUARTERS;
    }
    if (outCapacity != NULL) {
        *outCapacity = (uint16_t)((quarters / 16u) * 16u);
    }
    if (outPieces != NULL) {
        *outPieces = (uint16_t)((quarters % 16u) / 4u);
    }
}

// ============================================================================
// "IS THIS A REAL LOADED FILE" — the harvest gate. See the header for why this
// is mandatory: the title screen's gSaveContext is the ATTRACT DEMO's debug
// save, and F10 is polled ungated every frame.
//
// Kept here, in the game-header-free TU, so the POLICY is one testable function
// rather than two hand-written conditions that can drift apart in two shims.
// ============================================================================

bool Combo_SaveIsLiveFile(GameId game, int32_t gameMode) {
    if (!IsRealGame(game)) {
        return false;
    }

    // Gameplay only, and SYMMETRICALLY for both games — see the header for why
    // an OoT-only fileNum refinement would be worse than none. Every menu path
    // in both games (title, attract demo, file select) leaves a synthetic save
    // resident under a non-NORMAL mode, and both games set NORMAL at the
    // cross-game arrival itself, in the same block that calls their apply.
    //
    // MM's OWL_SAVE is the one accepted mode that is not gameplay: it is a real
    // file mid-save, entered from gameplay, and one of MM's two `.redsave`
    // capture points fires inside it. END_CREDITS is excluded in both — the save
    // is real but the session is terminal, so no later arrival can consume what
    // a harvest there would contribute.
    return gameMode == RSBS_GAMEMODE_NORMAL || (game == GAME_MM && gameMode == RSBS_GAMEMODE_OWL_SAVE);
}

bool Combo_SharedResourceKindArmed(uint8_t kind) {
    // ONE conditional kind, and the condition is the FROZEN world rule, not a
    // live preference. Everything else #525 decided for every world, except
    // the retired ocarina below.
    //
    // Deliberately a positive allowlist of the conditional kinds rather than a
    // `switch` returning false by default: a kind appended without a row here
    // must keep working, because the default for a shared resource is
    // unconditional and a silently disarmed kind is invisible — it looks exactly
    // like a player who has not found the item.
    //
    // The ocarina is NEVER armed (#879, operator ruling 2026-10-09). MM starts
    // every paired world holding its Ocarina of Time, so #668's Shared Ocarina
    // rule only ever carried that free instrument to Hyrule. The kind stays
    // allocated (.redsave format) and an older world's record may still carry
    // RSBS_COMBO_FLAG_SHARED_OCARINA; neither arms it, so its harvest and its
    // apply are no-ops and an older save's slot is inert.
    if (kind == (uint8_t)RSBS_SHARED_RES_OCARINA_TIER) {
        return false;
    }
    // ADR 0010 O10: the frozen goal decides, read from the record, never a CVar.
    // There is no live-resolution fallback: before a world exists there is no
    // hunt to share a count for.
    if (kind == (uint8_t)RSBS_SHARED_RES_TRIFORCE_PIECES) {
        return Combo_TriforceHuntArmed();
    }
    // #886: the Shared Kokiri Sword option, read from the FROZEN record's
    // comboFlags bit. Off (the default), neither game's sword is harvested or
    // applied and a world has no slot of this kind.
    if (kind == (uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD) {
        return Combo_ComboSharedKokiriSword();
    }
    // #970: the Shared Bottles rule, read from the FROZEN item rules record.
    // Off (the default), neither game's bottles are harvested or applied and a
    // world has no slot of this kind.
    if (kind == (uint8_t)RSBS_SHARED_RES_BOTTLE_COUNT) {
        return Combo_ComboCrossItemRule((uint32_t)RSBS_XITEM_SHARED_BOTTLES);
    }
    return true;
}

void Combo_HarvestSharedResource(GameId game, uint8_t kind, uint16_t liveValue) {
    if (!IsRealGame(game) || !IsRealKind(kind)) {
        return;
    }
    // THE ARMING GATE, half one of two (#668). Before any slot is created and
    // before any watermark is touched, so a disarmed kind leaves BOTH stores
    // exactly as it found them — the same clean-no-op discipline
    // Combo_SaveIsLiveFile's early return keeps, and for the same reason: a
    // world that never asked for this resource must not grow a slot for it.
    if (!Combo_SharedResourceKindArmed(kind)) {
        return;
    }

    // Read occupancy BEFORE any slot is created: "has this resource ever been
    // shared" is the discriminator the first-harvest seed turns on, and
    // FindOrCreateSlot would destroy the answer.
    const bool everShared = (FindSlot(kind) >= 0);

    if (IsMonotonicKind(kind)) {
        if (!everShared && liveValue == 0) {
            // Nothing to record. Deliberately does NOT create the slot: every
            // game harvests its whole resource set at each suspend, and a
            // player who has no wallet upgrade and no double defense yet would
            // otherwise occupy slots with zeros that read as "shared" — making
            // a fresh world indistinguishable from a played one, which is the
            // growth contract's "zero means unset" rule turned inside out.
            return;
        }
        const int slot = FindOrCreateSlot(kind);
        if (slot < 0) {
            return;
        }
        ComboSharedResource* entry = SlotAt(slot);
        if (liveValue > entry->value) {
            entry->value = liveValue;
        }
        return;
    }

    // CONSUMABLE: delta against the watermark, never a raw copy.
    SharedResWatermark* wm = &sWatermarks[kind];
    if (wm->owner != (uint8_t)game) {
        // No live watermark for this game — either a cold boot or a .redsave
        // load. Seed per the header's rule: an occupied slot means the pool was
        // written by a harvest that already counted this balance (seed at live,
        // delta zero), an empty slot means nothing was ever shared (seed at
        // zero, so the whole balance enters the pool).
        wm->value = everShared ? liveValue : 0u;
        wm->owner = (uint8_t)game;
    }

    const int32_t delta = (int32_t)liveValue - (int32_t)wm->value;
    wm->value = liveValue;
    if (delta == 0) {
        // Nothing changed. Deliberately does NOT create the slot: an untouched
        // resource stays unshared, which keeps a zero-extended legacy record
        // and a genuinely-never-used resource indistinguishable, as the growth
        // contract requires.
        return;
    }

    int slot = FindOrCreateSlot(kind);
    if (slot < 0) {
        return;
    }
    ComboSharedResource* entry = SlotAt(slot);
    int32_t next = (int32_t)entry->value + delta;
    if (next < 0) {
        next = 0;
    }
    if (next > 0xFFFF) {
        next = 0xFFFF;
    }
    entry->value = (uint16_t)next;
}

bool Combo_ApplySharedResource(GameId game, uint8_t kind, uint16_t cap, uint16_t* liveValue) {
    if (!IsRealGame(game) || !IsRealKind(kind) || liveValue == NULL) {
        return false;
    }
    // THE ARMING GATE, half two of two (#668). Symmetric with the harvest by
    // construction: both halves call the same predicate, so the pair cannot be
    // gated one-sidedly. A disarmed apply answers exactly as a never-shared
    // resource does — false, *liveValue untouched, no watermark — which is the
    // honest answer and the one every caller already handles. A pool populated
    // under a different world's rules is therefore inert rather than half-read.
    if (!Combo_SharedResourceKindArmed(kind)) {
        return false;
    }

    const int slot = FindSlot(kind);
    if (slot < 0) {
        // Never shared: nothing to apply, and deliberately NO watermark write.
        // Seeding lives in exactly one place — the harvest rule above — because
        // two seeding rules is how the halves disagree. Leaving the watermark
        // unset here means the next harvest applies that one rule and sees an
        // empty slot, so the game's existing balance joins the pool as genuinely
        // new money. That is the same answer a cold boot gets, which is what
        // makes "which side went first" stop mattering.
        return false;
    }

    const uint16_t shared = SlotAt(slot)->value;
    const uint16_t applied = (shared > cap) ? cap : shared;

    if (IsMonotonicKind(kind)) {
        // Raise only. A capacity the arriving game already exceeds is its own
        // business and its next harvest folds it back into the pool.
        if (applied > *liveValue) {
            *liveValue = applied;
        }
        return true;
    }

    // CONSUMABLE. Record what was ACTUALLY materialized, not what the pool
    // holds: the difference is the money that does not fit in this game's
    // wallet, and it must stay in the pool rather than be harvested away.
    *liveValue = applied;
    sWatermarks[kind].value = applied;
    sWatermarks[kind].owner = (uint8_t)game;
    return true;
}

bool Combo_SharedResourceCreditsAtPickup(uint8_t kind) {
    // #1061: the one counted kind whose packs cross (see the header). A positive
    // allowlist: a kind credited here must have its pack sizes published by both
    // games' item class sources, and today only the bombchu rows do.
    return kind == (uint8_t)RSBS_SHARED_RES_BOMBCHU_COUNT;
}

bool Combo_SharedPickupCreditOf(SharedItem item, uint8_t* outKind, uint16_t* outUnits) {
    const uint8_t kind = Combo_ItemClassSharedKind(item);
    // A counted pack (#1061) or a capacity tier (#1120): the shim tells them
    // apart by Combo_SharedResourceStepsAtPickup and runs the matching primitive.
    if (!Combo_SharedResourceCreditsAtPickup(kind) && !Combo_SharedResourceStepsAtPickup(kind)) {
        return false;
    }
    const uint8_t units = Combo_ItemClassSharedUnits(item);
    if (units == 0u) {
        return false;
    }
    if (outKind != NULL) {
        *outKind = kind;
    }
    if (outUnits != NULL) {
        *outUnits = (uint16_t)units;
    }
    return true;
}

bool Combo_CreditSharedResource(uint8_t kind, uint16_t amount) {
    // Consumable only, by construction: the allowlist holds no monotonic kind,
    // and a capacity tier is never "added to".
    if (!IsRealKind(kind) || amount == 0u || !Combo_SharedResourceCreditsAtPickup(kind) || IsMonotonicKind(kind)) {
        return false;
    }
    // The same arming gate as both halves above, so a disarmed kind grows no slot.
    if (!Combo_SharedResourceKindArmed(kind)) {
        return false;
    }
    const int slot = FindOrCreateSlot(kind);
    if (slot < 0) {
        return false;
    }
    ComboSharedResource* entry = SlotAt(slot);
    const uint32_t next = (uint32_t)entry->value + (uint32_t)amount;
    entry->value = (uint16_t)(next > 0xFFFFu ? 0xFFFFu : next);
    return true;
}

bool Combo_SharedResourceStepsAtPickup(uint8_t kind) {
    // #1120: the MONOTONIC kinds a far game's pool item feeds (the item class
    // sources, ComboLogicEngineOoT.cpp / ComboLogicEngineSingleExe.cpp). A
    // positive allowlist, like the counted one above. NOT the stick and nut
    // tiers: only OoT's Progressive Stick/Nut Upgrade feeds them, and OoT's
    // apply blocks their grant at tier 0 under Shuffle Deku Stick/Nut Bag
    // (OoT_SharedAmmoGrantBlocked) because MM is born at tier 1 — OoT's own
    // give is the only thing that opens that gate, so the item must reach OoT
    // as a crossing. NOT the ocarina (never armed) or the triforce count (no
    // pool item feeds it).
    switch (kind) {
        case RSBS_SHARED_RES_WALLET_TIER:
        case RSBS_SHARED_RES_HEALTH_QUARTERS:
        case RSBS_SHARED_RES_DOUBLE_DEFENSE:
        case RSBS_SHARED_RES_MAGIC_LEVEL:
        case RSBS_SHARED_RES_QUIVER_TIER:
        case RSBS_SHARED_RES_BOMB_BAG_TIER:
        case RSBS_SHARED_RES_HOOKSHOT_TIER:
        case RSBS_SHARED_RES_BOTTLE_COUNT:
        // #1145: OoT's RG_KOKIRI_SWORD and MM's RI_PROGRESSIVE_SWORD feed it.
        // Armed only by the frozen Shared Kokiri Sword bit (the step's gate), and
        // an MM copy only while the pool holds no sword (Combo_SharedPickupStepAdmitted).
        case RSBS_SHARED_RES_KOKIRI_SWORD:
            return true;
        default:
            return false;
    }
}

bool Combo_SharedPickupStepAdmitted(SharedItem item, uint8_t kind) {
    if (kind != (uint8_t)RSBS_SHARED_RES_KOKIRI_SWORD || item.originGame != (uint8_t)GAME_MM) {
        return true;
    }
    // MM's progressive sword: only the copy that finds the pool empty is the
    // shared Kokiri Sword. A never-shared slot reads as empty.
    uint16_t held = 0u;
    return !Combo_GetSharedResource(kind, &held) || held == 0u;
}

// The pool's ceiling for a stepped kind: the HIGHER of the two games' give
// ceilings, as the kind's quantity policy publishes them (shared_items.c; 0 for
// a kind without a tier budget, e.g. the health quarters), raised to the
// finder's own apply cap. See Combo_StepSharedTier's doc for why the finder's
// cap alone under-caps the hookshot.
static uint16_t StepCapOf(uint8_t kind, uint16_t finderCap) {
    ComboSharedQuantityPolicy policy;
    uint16_t cap = finderCap;
    if (Combo_SharedQuantityPolicyOf(kind, &policy)) {
        if ((uint16_t)policy.ceilingOoT > cap) {
            cap = (uint16_t)policy.ceilingOoT;
        }
        if ((uint16_t)policy.ceilingMM > cap) {
            cap = (uint16_t)policy.ceilingMM;
        }
    }
    return cap;
}

bool Combo_StepSharedTier(uint8_t kind, uint16_t units, uint16_t cap) {
    // Monotonic only, by construction: the twin of Combo_CreditSharedResource's
    // refusal of every monotonic kind, from the other side. A consumable is
    // CREDITED (added to, behind a watermark); a capacity is STEPPED (max-then-
    // bound), and neither primitive admits the other's kinds.
    if (!IsRealKind(kind) || units == 0u || !IsMonotonicKind(kind)) {
        return false;
    }
    // The same arming gate as the harvest and the apply, so a disarmed kind
    // grows no slot and the caller leaves the pickup to the crossing.
    if (!Combo_SharedResourceKindArmed(kind)) {
        return false;
    }
    // One copy in the kind's own units. The item class owner counts hearts in
    // PIECES (shared_items.h: one piece, four for a container); the pool counts
    // them in quarters, four per piece (Combo_MakeHealthQuarters).
    uint32_t step = units;
    if (kind == (uint8_t)RSBS_SHARED_RES_HEALTH_QUARTERS) {
        step *= 4u;
    }
    const uint32_t bound = StepCapOf(kind, cap);
    if (bound == 0u) {
        return false; // no ceiling published for this kind here: nothing to step toward, no zero slot
    }
    const int slot = FindOrCreateSlot(kind);
    if (slot < 0) {
        return false;
    }
    ComboSharedResource* entry = SlotAt(slot);
    uint32_t next = (uint32_t)entry->value + step;
    if (next > bound) {
        next = bound; // the copy is absorbed at the cap; never lowers the slot
    }
    if (next > entry->value) {
        entry->value = (uint16_t)next;
    }
    return true;
}

void Combo_FreezeFillWalletsAtCreation(bool on) {
    gComboCtx.comboFillWallets = on ? 1u : 0u;
}

bool Combo_SharedWalletFillRule(void) {
    // The frozen byte, never OoT's seed settings: MM asks the same question
    // while OoT's settings are not its to read, and the answer is the pair's.
    return Combo_ComboSettingsFrozen() && gComboCtx.comboFillWallets != 0u;
}

bool Combo_GetSharedResource(uint8_t kind, uint16_t* outValue) {
    if (!IsRealKind(kind) || outValue == NULL) {
        return false;
    }
    const int slot = FindSlot(kind);
    if (slot < 0) {
        return false;
    }
    *outValue = SlotAt(slot)->value;
    return true;
}

int Combo_CountSharedResources(void) {
    int count = 0;
    for (int i = 0; i < (int)RSBS_SHARED_RESOURCE_TOTAL_CAP; i++) {
        if (SlotAt(i)->kind != RSBS_SHARED_RES_NONE) {
            count++;
        }
    }
    return count;
}

void Combo_ResetSharedResourceWatermarks(void) {
    memset(sWatermarks, 0, sizeof(sWatermarks));
}

// ============================================================================
// SHARED FAMILIES, the OR discipline (#976). See shared_resources.h.
// ============================================================================

/** One registered native copy per (game, family SLOT): slot = word * 32 + bit
 *  (RSBS_XFAMILY_SLOT_COUNT, foreign_items.h), slots 0-31 the first family word,
 *  32-63 the second (#1169 XW10). RAM only: a registration is a property of this
 *  build's code, not of the saved world. */
static ComboSharedFamilyMember sFamilyMembers[3][RSBS_XFAMILY_SLOT_COUNT];
static bool sFamilyMemberSet[3][RSBS_XFAMILY_SLOT_COUNT];

/** The bit index of a single FAMILY SLOT (a family rule's own bit, or an extra
 *  slot such as #1068's Mirror Shield), or -1 for anything else. */
static int FamilyIndex(uint32_t familyBit) {
    if (familyBit == 0u || (familyBit & (familyBit - 1u)) != 0u ||
        (familyBit & (uint32_t)RSBS_XFAMILY_SLOTS_ALL) == 0u) {
        return -1;
    }
    for (int b = 0; b < (int)RSBS_XITEM_BIT_COUNT; ++b) {
        if (familyBit == (1u << b)) {
            return b;
        }
    }
    return -1;
}

/** The SLOT of a single SECOND-word family bit (an RSBS_XITEM2_FAMILY_MASK bit):
 *  32 + its bit index, or -1 for anything else (an extension rule, an unallocated
 *  bit, a mask with more than one bit) (#1169 XW10). */
static int Family2Index(uint32_t family2Bit) {
    if (family2Bit == 0u || (family2Bit & (family2Bit - 1u)) != 0u ||
        (family2Bit & (uint32_t)RSBS_XITEM2_FAMILY_MASK) == 0u) {
        return -1;
    }
    for (int b = 0; b < (int)RSBS_XITEM2_BIT_COUNT; ++b) {
        if (family2Bit == (1u << b)) {
            return (int)RSBS_XITEM_BIT_COUNT + b;
        }
    }
    return -1;
}

static bool FamilyRegisterAt(GameId game, int slot, const ComboSharedFamilyMember* member) {
    if (!IsRealGame(game) || slot < 0) {
        return false;
    }
    if (member == NULL) {
        sFamilyMemberSet[game][slot] = false;
        memset(&sFamilyMembers[game][slot], 0, sizeof(sFamilyMembers[game][slot]));
        return true;
    }
    if (member->held == NULL || member->give == NULL) {
        return false;
    }
    sFamilyMembers[game][slot] = *member;
    sFamilyMemberSet[game][slot] = true;
    return true;
}

static bool FamilyMemberIdAt(GameId game, int slot, uint16_t* outId) {
    if (!IsRealGame(game) || slot < 0 || !sFamilyMemberSet[game][slot]) {
        return false;
    }
    if (outId != NULL) {
        *outId = sFamilyMembers[game][slot].nativeId;
    }
    return true;
}

static bool FamilyMemberGetAt(GameId game, int slot, ComboSharedFamilyMember* out) {
    if (!IsRealGame(game) || slot < 0 || !sFamilyMemberSet[game][slot]) {
        return false;
    }
    if (out != NULL) {
        *out = sFamilyMembers[game][slot];
    }
    return true;
}

static bool FamilyMemberPooledAt(GameId game, int slot) {
    if (!IsRealGame(game) || slot < 0 || !sFamilyMemberSet[game][slot]) {
        return false;
    }
    const ComboSharedFamilyMember* m = &sFamilyMembers[game][slot];
    return m->pooled == NULL || m->pooled(m->nativeId);
}

bool Combo_RegisterSharedFamilyMember(GameId game, uint32_t familyBit, const ComboSharedFamilyMember* member) {
    return FamilyRegisterAt(game, FamilyIndex(familyBit), member);
}

bool Combo_SharedFamilyMemberId(GameId game, uint32_t familyBit, uint16_t* outId) {
    return FamilyMemberIdAt(game, FamilyIndex(familyBit), outId);
}

bool Combo_SharedFamilyMemberGet(GameId game, uint32_t familyBit, ComboSharedFamilyMember* out) {
    return FamilyMemberGetAt(game, FamilyIndex(familyBit), out);
}

bool Combo_SharedFamilyMemberPooled(GameId game, uint32_t familyBit) {
    return FamilyMemberPooledAt(game, FamilyIndex(familyBit));
}

bool Combo_RegisterSharedFamily2Member(GameId game, uint32_t family2Bit, const ComboSharedFamilyMember* member) {
    return FamilyRegisterAt(game, Family2Index(family2Bit), member);
}

bool Combo_SharedFamily2MemberId(GameId game, uint32_t family2Bit, uint16_t* outId) {
    return FamilyMemberIdAt(game, Family2Index(family2Bit), outId);
}

bool Combo_SharedFamily2MemberGet(GameId game, uint32_t family2Bit, ComboSharedFamilyMember* out) {
    return FamilyMemberGetAt(game, Family2Index(family2Bit), out);
}

bool Combo_SharedFamily2MemberPooled(GameId game, uint32_t family2Bit) {
    return FamilyMemberPooledAt(game, Family2Index(family2Bit));
}

void Combo_ClearSharedFamilyMembers(void) {
    // Both family words: the arrays are sized by RSBS_XFAMILY_SLOT_COUNT.
    memset(sFamilyMembers, 0, sizeof(sFamilyMembers));
    memset(sFamilyMemberSet, 0, sizeof(sFamilyMemberSet));
}

uint32_t Combo_SharedFamilySlotsArmedBy(uint32_t crossItemFlags) {
    uint32_t slots = crossItemFlags & (uint32_t)RSBS_XITEM_FAMILY_MASK;
    // A rule with more than one pair arms its extra slots with its own bit
    // (#1068: the shields rule's second pair, the two Mirror Shields).
    if ((crossItemFlags & (uint32_t)RSBS_XITEM_SHARED_SHIELDS) != 0u) {
        slots |= (uint32_t)RSBS_XFAMILY_SLOT_MIRROR_SHIELD;
    }
    // #1067: the shared ocarina buttons' four C-button pairs.
    if ((crossItemFlags & (uint32_t)RSBS_XITEM_SHARED_OCARINA_BUTTONS) != 0u) {
        slots |= (uint32_t)(RSBS_XFAMILY_SLOT_OCARINA_C_UP | RSBS_XFAMILY_SLOT_OCARINA_C_DOWN |
                            RSBS_XFAMILY_SLOT_OCARINA_C_LEFT | RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT);
    }
    return slots;
}

const char* Combo_SharedFamilySlotName(uint32_t slot) {
    if (slot == (uint32_t)RSBS_XFAMILY_SLOT_MIRROR_SHIELD) {
        return "sharedShields (Mirror Shield)";
    }
    // An extra slot at an extension rule's bit names its pair, not that rule.
    switch (slot) {
        case RSBS_XFAMILY_SLOT_OCARINA_C_UP:
            return "sharedOcarinaButtons (C-Up)";
        case RSBS_XFAMILY_SLOT_OCARINA_C_DOWN:
            return "sharedOcarinaButtons (C-Down)";
        case RSBS_XFAMILY_SLOT_OCARINA_C_LEFT:
            return "sharedOcarinaButtons (C-Left)";
        case RSBS_XFAMILY_SLOT_OCARINA_C_RIGHT:
            return "sharedOcarinaButtons (C-Right)";
        case RSBS_XITEM_SHARED_OCARINA_BUTTONS:
            return "sharedOcarinaButtons (A)";
        default:
            break;
    }
    return Combo_ComboItemRuleName(slot);
}

const char* Combo_SharedFamily2SlotName(uint32_t family2Bit) {
    // A second-word family bit IS its rule bit, so the rule's OoTMM key names it.
    return Combo_ComboItemRule2Name(family2Bit);
}

uint32_t Combo_SharedFamiliesArmed(void) {
    // The FROZEN record only, as every arming gate in this file: before a world
    // exists there is nothing to share, and after it the rules are identity.
    return Combo_SharedFamilySlotsArmedBy(Combo_ComboCrossItemFlags());
}

uint32_t Combo_SharedFamiliesOwned(void) {
    return gComboCtx.sharedFamilyOwned;
}

uint32_t Combo_SharedFamiliesHarvest(GameId game, uint32_t heldBits) {
    if (!IsRealGame(game)) {
        return gComboCtx.sharedFamilyOwned;
    }
    // OR, never assign: a family the departing game no longer holds keeps its
    // bit, so the other game's copy is never taken away.
    gComboCtx.sharedFamilyOwned |= heldBits & Combo_SharedFamiliesArmed();
    return gComboCtx.sharedFamilyOwned;
}

uint32_t Combo_SharedFamiliesOwed(GameId game, uint32_t heldBits) {
    if (!IsRealGame(game)) {
        return 0u;
    }
    return gComboCtx.sharedFamilyOwned & Combo_SharedFamiliesArmed() & ~heldBits;
}

uint32_t Combo_SharedFamilies2Armed(void) {
    // The FROZEN second word only, never a CVar: its family bits are its rule bits.
    return Combo_ComboCrossItemFlags2() & (uint32_t)RSBS_XITEM2_FAMILY_MASK;
}

uint32_t Combo_SharedFamilies2Owned(void) {
    return gComboCtx.sharedFamilyOwned2;
}

uint32_t Combo_SharedFamilies2Harvest(GameId game, uint32_t heldBits) {
    if (!IsRealGame(game)) {
        return gComboCtx.sharedFamilyOwned2;
    }
    // OR, never assign, armed bits only: the first word's discipline.
    gComboCtx.sharedFamilyOwned2 |= heldBits & Combo_SharedFamilies2Armed();
    return gComboCtx.sharedFamilyOwned2;
}

uint32_t Combo_SharedFamilies2Owed(GameId game, uint32_t heldBits) {
    if (!IsRealGame(game)) {
        return 0u;
    }
    return gComboCtx.sharedFamilyOwned2 & Combo_SharedFamilies2Armed() & ~heldBits;
}

/** The families of family word @p word (0 the first, 1 the second) that @p game's
 *  registered members say its live save holds, armed families only (a disarmed
 *  family's callback is never called). */
static uint32_t FamiliesHeldLive(GameId game, int word, uint32_t armed) {
    uint32_t held = 0u;
    for (int b = 0; b < (int)RSBS_XITEM_BIT_COUNT; ++b) {
        const uint32_t bit = 1u << b;
        const int slot = word * (int)RSBS_XITEM_BIT_COUNT + b;
        if ((armed & bit) == 0u || !sFamilyMemberSet[game][slot]) {
            continue;
        }
        const ComboSharedFamilyMember* m = &sFamilyMembers[game][slot];
        if (m->held(m->nativeId)) {
            held |= bit;
        }
    }
    return held;
}

// #1169 XW14: each game's live-file probe (Combo_RegisterSharedLiveProbe).
// Zero-initialised storage, written by static registrars: safe whatever the
// static-init order.
static ComboSharedLiveProbeFn sSharedLiveProbes[3];

void Combo_RegisterSharedLiveProbe(GameId game, ComboSharedLiveProbeFn probe) {
    if (IsRealGame(game)) {
        sSharedLiveProbes[game] = probe;
    }
}

ComboSharedLiveProbeFn Combo_SharedLiveProbeFor(GameId game) {
    return IsRealGame(game) ? sSharedLiveProbes[game] : NULL;
}

uint32_t Combo_SharedFamilies2HeldNow(void) {
    const uint32_t armed2 = Combo_SharedFamilies2Armed();
    if (armed2 == 0u) {
        return 0u; // every world with no shared rule frozen: no callback runs
    }
    // owned2 holds every copy harvested at a suspend or a .redsave capture; the
    // running game's own live copy is the only one it can be missing (OoTMM's
    // has(SHARED_SONG_X)). A member is asked only under its own game's probe, so
    // never for the other game's shadow, a title screen or a file select.
    uint32_t held = gComboCtx.sharedFamilyOwned2 & armed2;
    const GameId games[2] = { GAME_OOT, GAME_MM };
    for (int i = 0; i < 2; ++i) {
        const ComboSharedLiveProbeFn probe = sSharedLiveProbes[games[i]];
        if (probe != NULL && probe() != 0) {
            held |= FamiliesHeldLive(games[i], 1, armed2);
        }
    }
    return held;
}

/** #1169 XW14: the second-word families Combo_SharedFamiliesApplyLive GAVE each game
 *  on its most recent call (Combo_SharedFamilies2GivenAtLastApply). Session state,
 *  file-static: never ComboContext, never saved. */
static uint32_t sFamilies2GivenAtLastApply[3];

/** Give @p game its own copy of every family of word @p word in @p owed. */
static int FamiliesGiveOwed(GameId game, int word, uint32_t owed) {
    int given = 0;
    for (int b = 0; b < (int)RSBS_XITEM_BIT_COUNT; ++b) {
        const uint32_t bit = 1u << b;
        const int slot = word * (int)RSBS_XITEM_BIT_COUNT + b;
        if ((owed & bit) == 0u || !sFamilyMemberSet[game][slot]) {
            continue;
        }
        const ComboSharedFamilyMember* m = &sFamilyMembers[game][slot];
        m->give(m->nativeId);
        given++;
        if (word == 1) {
            sFamilies2GivenAtLastApply[game] |= bit;
        }
        fprintf(stderr, "[combo] shared family %s: %s given its own copy (id %u) at arrival\n",
                word == 0 ? Combo_SharedFamilySlotName(bit) : Combo_SharedFamily2SlotName(bit), Game_ToString(game),
                (unsigned)m->nativeId);
    }
    return given;
}

uint32_t Combo_SharedFamiliesHarvestLive(GameId game) {
    if (!IsRealGame(game)) {
        return gComboCtx.sharedFamilyOwned;
    }
    // #1169 XW10: both family words. Each is asked only when it arms a family, so a
    // world that arms neither (the default) runs no callback, and a world that arms
    // only the second word still harvests it.
    const uint32_t armed = Combo_SharedFamiliesArmed();
    const uint32_t armed2 = Combo_SharedFamilies2Armed();
    if (armed == 0u && armed2 == 0u) {
        return gComboCtx.sharedFamilyOwned; // the default world: no callback runs
    }
    if (armed != 0u) {
        (void)Combo_SharedFamiliesHarvest(game, FamiliesHeldLive(game, 0, armed));
    }
    if (armed2 != 0u) {
        (void)Combo_SharedFamilies2Harvest(game, FamiliesHeldLive(game, 1, armed2));
    }
    return gComboCtx.sharedFamilyOwned;
}

int Combo_SharedFamiliesApplyLive(GameId game) {
    if (!IsRealGame(game)) {
        return 0;
    }
    // #1169 XW14: this call's give record starts empty, before any early return, so
    // an apply that owes nothing reads 0 afterwards.
    sFamilies2GivenAtLastApply[game] = 0u;
    // #1169 XW10: a family word is applied when it arms a family and the pool holds
    // one of it; the first word exactly as before, then the second.
    const uint32_t armed = Combo_SharedFamiliesArmed();
    const uint32_t armed2 = Combo_SharedFamilies2Armed();
    const bool word1 = armed != 0u && gComboCtx.sharedFamilyOwned != 0u;
    const bool word2 = armed2 != 0u && gComboCtx.sharedFamilyOwned2 != 0u;
    if (!word1 && !word2) {
        return 0;
    }
    int given = 0;
    if (word1) {
        given += FamiliesGiveOwed(game, 0, Combo_SharedFamiliesOwed(game, FamiliesHeldLive(game, 0, armed)));
    }
    if (word2) {
        given += FamiliesGiveOwed(game, 1, Combo_SharedFamilies2Owed(game, FamiliesHeldLive(game, 1, armed2)));
    }
    return given;
}

uint32_t Combo_SharedFamilies2GivenAtLastApply(GameId game) {
    return IsRealGame(game) ? sFamilies2GivenAtLastApply[game] : 0u;
}

const char* Combo_SharedFamily2DisplayName(uint8_t game, uint16_t nativeId) {
    const GameId g = (GameId)game;
    const uint32_t armed2 = Combo_SharedFamilies2Armed();
    if (!IsRealGame(g) || armed2 == 0u) {
        return NULL; // every world with no shared rule frozen: the names stay
    }
    for (int b = 0; b < (int)RSBS_XITEM2_BIT_COUNT; ++b) {
        const uint32_t bit = 1u << b;
        const int slot = (int)RSBS_XITEM_BIT_COUNT + b;
        if ((armed2 & bit) == 0u || !sFamilyMemberSet[g][slot] || sFamilyMembers[g][slot].nativeId != nativeId) {
            continue;
        }
        const uint32_t ext = Combo_ComboItemRule2Requires(bit);
        if (ext == (uint32_t)RSBS_XITEM2_SONG_SOARING_OOT) {
            return "Song of Soaring";
        }
        return Combo_MMWarpSongOoTName(ext); // NULL for a family that is no warp song
    }
    return NULL;
}

bool Combo_SharedFamilies2CreditFarPickup(GameId running, uint8_t originGame, uint16_t id) {
    const GameId origin = (GameId)originGame;
    if (!IsRealGame(running) || !IsRealGame(origin) || origin == running) {
        return false;
    }
    const uint32_t armed2 = Combo_SharedFamilies2Armed();
    for (int b = 0; armed2 != 0u && b < (int)RSBS_XITEM2_BIT_COUNT; ++b) {
        const uint32_t bit = 1u << b;
        const int slot = (int)RSBS_XITEM_BIT_COUNT + b;
        if ((armed2 & bit) == 0u || !sFamilyMemberSet[origin][slot] || sFamilyMembers[origin][slot].nativeId != id) {
            continue;
        }
        // The far game's copy of an ARMED family: the pool owns the family now (the
        // origin game gets its own copy at its next arrival, through the apply), and
        // the running game gets its own copy at once.
        gComboCtx.sharedFamilyOwned2 |= bit & armed2;
        if (sFamilyMemberSet[running][slot]) {
            const ComboSharedFamilyMember* m = &sFamilyMembers[running][slot];
            if (!m->held(m->nativeId)) {
                m->give(m->nativeId);
            }
        }
        fprintf(stderr, "[combo] shared family %s: %s's copy (id %u) found in %s, credited at pickup, no crossing\n",
                Combo_SharedFamily2SlotName(bit), Game_ToString(origin), (unsigned)id, Game_ToString(running));
        return true;
    }
    return false;
}
