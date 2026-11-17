/**
 * @file oot_logic_gates_test.cpp
 * @brief #1168: the talking and grabbing gates Ship of Harkinian added to its
 *        location logic after our import base, driven through OoT's REAL search
 *        primitives. CTest row `OoTLogicGates` (label `rando`), test name
 *        `oot-logic-gates`, registered in src/common/test_runner.cpp.
 *
 * Every leg evaluates the location exactly the way the search does for one
 * region (3drando/fill.cpp's ReachabilitySearch): the region is given its
 * age/time access, its events are fired to a fixpoint with Region::UpdateEvents,
 * and the location is asked LocationAccess::ConditionsMet(region, false) with
 * logic->CurrentCheckKey set to it. The lambdas are the ones RegionTable_Init
 * builds — never a re-statement of a condition, which would pass with the gate
 * deleted.
 *
 * The world is Shuffle Speak ON and Shuffle Grab ON (forced here, restored on
 * exit), so Logic::Reset grants neither ability and each leg says what it holds.
 *
 *  A. Song from Malon (RC_SONG_FROM_MALON) needs Malon home from the castle
 *     (LOGIC_MALON_RETURNED_FROM_CASTLE; #1404 / SoH #6938, which replaced the
 *     Zelda's Letter proxy) and the Hylian jabber nut: a child by day with the
 *     Fairy Ocarina, the nut and Zelda's Letter but Malon not home must NOT reach
 *     it; with Malon home and the ocarina but no nut must NOT (SoH c906c71f7);
 *     with the nut too it must, holding no letter.
 *  B. Talon's chickens (RC_LLR_TALONS_CHICKENS) need Talon home
 *     (LOGIC_TALON_RETURNED_FROM_CASTLE, #1404) and the Power Bracelet (Shuffle
 *     Grab's grab ability): wallet, nut, bracelet and letter by day but Talon not
 *     home must NOT reach it; Talon home with wallet and nut but no bracelet must
 *     NOT (SoH 63597ec63, logic half); with the bracelet it must, holding no
 *     letter. Reaches fires only the probed region's events, so A and B set the
 *     castle events by hand after the reset.
 *  HC/RT. Where those two events come from (#1404): in the castle moat a child
 *     with the Weird Egg and the nut wakes Talon (not without the egg, not
 *     without the nut, not as an adult), and at the castle gate Talon's return
 *     plus the nut sends Malon home (neither alone); the moat's drain ledge needs
 *     Talon's return and the bracelet. At the root, Skip Waking Talon or the
 *     legacy Skip Child Zelda key sets both events with no egg and no letter;
 *     Vanilla or Shuffled with the legacy key off sets neither.
 *  C. The four Thieves' Hideout carpenters: a fighter without the nut must NOT
 *     reach a carpenter's check; with the nut the cell's own "could free" event
 *     fires and the check is reached. (SoH 1413ceb33, logic half.)
 *  D. The Haunted Wasteland carpet merchant (RC_WASTELAND_BOMBCHU_SALESMAN): a
 *     jumpslash alone used to satisfy it (`&&` binds tighter than `||`). Without
 *     the nut it must NOT be reached; with the nut and a price the wallet covers
 *     it must; with the nut and a price above the wallet it must NOT.
 *     (SoH 0702b11b9.)
 *  E. The four carpenters' check rows (#1191, SoH 2bc50f605): each cell's Gerudo
 *     guard (En_GeldB) drops its key as En_Item00 with params keyFlag | 0x11
 *     (z_en_geldb.c), and the randomizer names the check that drop awards by
 *     (actor, scene, params) through CheckFromActorMultimap and marks it
 *     collected by the scene collectible flag. Each carpenter's row must carry
 *     its OWN cell's guard key flag, and the location table must hold exactly
 *     one En_Item00 row for that drop, the carpenter's. The flags are the guards'
 *     params in the Thieves' Hideout rooms (gerudoway_room_N actor lists; rooms
 *     per randomizer_entrance_tracker.cpp): room 2 1 Torch 0x0C, room 1 Dead End
 *     0x0A, room 5 Double Cell 0x0F, room 4 Steep Slope 0x0E. The pots each row
 *     of ShufflePots.cpp names for a cell stand in the same room as its guard.
 *     A data leg: no search, no save.
 *
 *  F. The interior pots whose scene disables the item (#1282, SoH 041295f05):
 *     with Shuffle Pots and Shuffle Grab ON and grab withheld, CanBreakPots must
 *     not count an item the scene's OoT_sRestrictionFlags row disables
 *     (z_parameter.c). Link's House, the Twins' and Know-It-All Brothers' houses
 *     and the back-alley house disable B, the hookshot and every C item: a child
 *     with the Kokiri Sword, sticks, slingshot, boomerang and bombs must NOT
 *     reach their pots; with grab he must. Talon's house disables C items and
 *     the hookshot but not B: the C items alone must NOT, the Kokiri Sword must.
 *     The guard house disables only the hookshot and longshot: an adult with the
 *     longshot must NOT reach its adult pots; with a bomb bag he must. Control:
 *     the Kokiri Sword alone still breaks a Goron City pot (no restriction).
 *
 * #1277, Shuffle Climb ON as well (and Shuffle Open Chest ON, tricks forced
 * off unless a leg names one; all restored): the ladders and the neighbouring
 * gates Ship of Harkinian re-gated after our import base (bundle O5).
 *  F. Kak Backyard (b6cdc6c49): no exit beyond the three ground-level ones
 *     opens for an adult by day without Climb (the potion shop's back side is up
 *     a ladder); one does with it. The watchtower jump to the rooftop is any age
 *     under RT_UNINTUITIVE_JUMPS and closed without it.
 *  G. DMC wall Piece of Heart (ec27775e8, the PoH clause, in 7522f31cc's
 *     RR_DMC_UPPER_ENTRY): an adult with 3 hearts must NOT reach it without
 *     Climb; with Climb it must.
 *  H. DC Armos Room -> Bomb Room Lower (fe5dce24b): a child with the bracelet
 *     must NOT pass without Climb; with Climb it must.
 *  I. Child Spirit "before climb" (fe5dce24b): its locked door leads to Sun On
 *     Floor 1F, not 2F. A data leg.
 *  J. Deku Tree 2F grass (fe5dce24b): a child with a sword searching from the
 *     lobby must NOT cut the two 2F grass checks without Climb; with it, must.
 *  K. Deku Tree map chest (743103fbe): not from the lobby floor without Climb,
 *     not without Open Chest, reached with both.
 *  L. GV waterfall alcove (09b0fdbc9 without its RT_HOOKSHOT_LADDERS clause):
 *     the Longshot alone and the Hookshot plus Climb must NOT open it; the
 *     Longshot plus Climb must.
 *  M. SFM (ed4868f62): a child that never killed the Wolfos must NOT walk back
 *     out through the gate; after a search opened it, and as adult, it must.
 *  N. MQ DC torch room (848106039, CanClimbLadder read as HasItem(RG_CLIMB)): a
 *     child with nothing must NOT reach the Dodongo room from the torch room
 *     floor; with Climb, or with the bracelet, it must.
 *  O. Jabu MQ near-boss chest (7be5a4fea): the slingshot without Open Chest
 *     must NOT reach it; with Open Chest it must.
 *  P. MQ Spirit child doors (688913270): under RT_HOVER_BOOST_SIMPLE each door
 *     needs one key more (1 -> 2, 2 -> 3); without the trick one key still opens
 *     the first.
 *
 * #1300, RT_HOOKSHOT_LADDERS / CanClimbLadder / CanClimbHighLadder (SoH
 * 60ec65592, bundle O28, plus the two deferred clauses from 09b0fdbc9 and
 * 848106039):
 *  Q. GV waterfall alcove (09b0fdbc9): under RT_HOOKSHOT_LADDERS the Longshot
 *     without Climb opens it (locking 09b0fdbc9); the Hookshot without Climb
 *     still must NOT.
 *  R. Dodongo's Cavern MQ compass chest (60ec65592): behind the pushed block in
 *     the MQ Dodongo room, reaching the chest without killing the Dodongos
 *     needs CanClimbLadder (Climb, or Hookshot under RT_HOOKSHOT_LADDERS;
 *     closed on Hookshot with the trick off).
 *  S. Bottom of the Well (60ec65592): both the vanilla basement ladder
 *     (RR_BOTW_B3_OOZE -> RR_BOTW_LADDER_DOOR) and the MQ basement ladder
 *     (RR_BOTW_MQ_B3 -> RR_BOTW_MQ_PERIMETER) are CanClimbHighLadder (Climb, or
 *     Longshot under RT_HOOKSHOT_LADDERS; closed on Longshot with the trick off
 *     and on short Hookshot with the trick on).
 *  T. Shadow Temple dock (60ec65592, vanilla): RR_SHADOW_TEMPLE_DOCK ->
 *     RR_SHADOW_TEMPLE_BEYOND_BOAT requires RT_HOOKSHOT_LADDERS for the
 *     Hookshot path (closed on Hookshot with the trick off, open with it, and
 *     open with Goron's Bracelet + Climb).
 *  U. DMC and Hyrule Field (60ec65592): the crater ladder (7522f31cc's
 *     RR_DMC_BLOCKED_ENTRY -> RR_DMC_CRATE, Goron Tunic held) and both
 *     directions of the Hyrule Field <-> Lake Hylia fence ladder
 *     (RR_HYRULE_FIELD <-> RR_HF_TO_LAKE_HYLIA) are CanClimbLadder (closed on
 *     Hookshot with the trick off, open with it).
 *  V. Lost Woods and SFM (60ec65592): the Lost Woods under-bridge exit
 *     (RR_LW_UNDER_BRIDGE -> RR_THE_LOST_WOODS) and both SFM maze ladders
 *     (RR_SACRED_FOREST_MEADOW -> RR_SFM_ABOVE_MAZE and
 *     RR_SFM_OUTSIDE_FAIRY_GROTTO -> RR_SFM_ABOVE_MAZE) are CanClimbLadder
 *     (closed on Hookshot with the trick off, open with it).
 *  W. Gerudo Fortress (60ec65592): RR_GF_OUTSKIRTS -> RR_GF_TOWER is
 *     CanClimbHighLadder, where LOGIC_GF_GATE_OPEN fires; the wasteland gate
 *     sidehop skip (RT_GF_WASTELAND_GATE_SIDEHOP_SKIP) opens for a child.
 *  X. Zora's River (60ec65592): RR_ZORAS_RIVER -> RR_ZR_ATOP_LADDER replaces
 *     its unconditional Hookshot term with CanClimbLadder (closed on Hookshot
 *     with the trick off, open with it).
 *
 *  Note: the Hookshot terms in 848106039 (dodongos_cavern.cpp:428), ZD island
 *  (zoras_domain.cpp:49), DC Armos (dodongos_cavern.cpp:159), the BotW corridor
 *  (bottom_of_the_well.cpp:23) and the KF child branch (kokiri_forest.cpp:77)
 *  are ported faithfully but cannot change logic for either age because
 *  Logic::CanUse(RG_HOOKSHOT/RG_LONGSHOT) requires IsAdult (logic.cpp:829-831),
 *  so they are not lockable. Q2 locks 09b0fdbc9.
 *
 * #1397 (packet SLC-12 and SLC-13), at the x1 damage multiplier unless named:
 *  Y. Health (SoH b52c31f85): Logic::Reset seeds the logic save from Starting
 *     Hearts (one heart is health 16), and the region thresholds read
 *     EffectiveHealth in 16-per-heart units: a child with one heart must NOT
 *     leave the GC grotto platform through the lava (the dive costs a heart);
 *     with two he must. Three hearts are EffectiveHealth 48, and Double Defense
 *     doubles it to 96 (it must never shrink it); OHKO is 1.
 *  Z. DMT rockfall (SoH 91425a39a): a child with Climb but no shield, Nayru's
 *     Love or RT_DMT_SHIELDLESS_CLIMB must NOT reach the summit; an adult with
 *     Climb must, and so must the child once the trick is on.
 *  VC. Voidout Collection (SoH e86e0ff69, #1647), with Swim withheld and three
 *     hearts (CanVoid): every rule the trick opens is closed with it off and open
 *     with it on (HC moat, ZR wonder items, waterfall rupees and underwater rock,
 *     GV waterfall wonder items, Octorok grotto rupee and underwater rock, the
 *     Colossus oasis, the Deku Tree before-boss hearts, the Forest Trial silver
 *     rupees, the Shadow MQ Gold Skulltulas, the Thieves' Hideout kitchen pot and
 *     soup, and the MQ Fire Trial on-pillar silver rupee from each side of the
 *     Fire Trial split, VC55 to VC58). The rules that got STRICTER with the trick off: RC_OGC_GS needs the
 *     Hookshot or Boomerang (a sword alone no longer does), the GV waterfall
 *     wonder items need Swim, Iron Boots or the bean, and the Shadow MQ near-boss
 *     Gold Skulltula needs the Hookshot or Boomerang; LOOSER: the Deku Tree left
 *     and right before-boss hearts are an adult's with nothing. Under OHKO with no
 *     fairy the trick opens nothing (CanVoid).
 *
 * #1647's O28 crater lane (SoH 7522f31cc and 53dc7f43b, 69ef35d25, ec27775e8,
 * 46f775ef8, b52c31f85, ff0209e76, eca767dcb), tricks off unless a leg names one,
 * no tunic, Shuffle Climb off (restored):
 *  DX. Every crater region the refactor adds is built.
 *  DT. Every exit of the seven entry regions at its heart threshold: an adult
 *     with the Hover Boots (and the Longshot and the scarecrow for the far
 *     platform) passes with the threshold's hearts and NOT with one fewer; the
 *     wall Piece of Heart (Climb) and the volcano Piece of Heart
 *     (RT_DMC_HOVER_BEAN_POH) likewise from each entry.
 *  DF. The heat timer side: the Goron Tunic opens the upper entry to the centre
 *     with one heart; Fewer Tunic Requirements opens it with 8 hearts, not 7.
 *  DV. LOGIC_DMC_BOULDER: the blocked exit fires it for an adult with the hammer
 *     (not without); the scrub ledge only under RT_DMC_BOULDER_JS.
 *  DU. DMCUpperToPots: closed for an adult with nothing; open with the boulder
 *     event, with RT_DMC_BOULDER_SKIP; a child holding the hood only under Bunny
 *     Hood Effect (eca767dcb, #1647 G1).
 *  DP. DMCPotsToPad: the Hookshot; RT_DMC_BOLERO_JUMP with a shield and the Power
 *     Bracelet (not without the bracelet); a child with the hood and Climb only
 *     under Bunny Hood Effect.
 *  DQ. DMCPadToPots: the Hookshot; an adult with the hood and Climb only under
 *     Masks as Adult.
 *  DB. The bean: planted, an adult passes with 3 hearts.
 *  DS. The child's bean sprout fairy (upstream's form before 55b52a26a): a
 *     child alone in the crater with the soul and Song of Storms reaches it
 *     with a bean to plant, and not without one.
 *  DD. DMCUpperToPad: the scarecrow jump needs RT_UNINTUITIVE_JUMPS, and 9 hearts.
 *  DC. A child reaches the temple exit only under dungeon entrance shuffle.
 *  DE. The entrance table rows the refactor moved (entrance.cpp): each row's
 *     exit carries its index and type, and each two-way row's reverse is the
 *     exit upstream names (the Fire Temple's comes out at RR_DMC_TEMPLE_ENTRY).
 *
 * The SoH logic batch of 2026-11-14 (SoH 156c2a4a #7417, 28ab1206 #7396):
 *  LH. Lake Hylia grass (156c2a4a): an adult with the ocarina and Epona's Song
 *     but Epona not freed, and nothing that cuts, must NOT reach RC_LH_GRASS_1;
 *     with Epona freed (SummonEpona) he must.
 *  GV. GV rocks across the bridge (156c2a4a): an adult with nothing must NOT
 *     reach RC_GV_ROCK_ACROSS_BRIDGE_1; with a bomb bag (CanBreakRocks) he must.
 *  GC. MQ Fire Trial (28ab1206): the Final Room returns to the From Barred Door
 *     region, not straight to the main room; from there an adult with the Goron
 *     Tunic, hammer and Hover Boots must NOT cross back to the open door (or take
 *     the launch-platform silver rupee) without RT_HOVER_BOOST_SIMPLE, and must
 *     with it.
 *  FT. The Fire Trial split by door (SoH 139381a0e, 88099e6eb, develop's form):
 *     with no Goron Tunic the heart piece is in logic only under
 *     RT_FEWER_TUNIC_REQUIREMENTS with enough health (FireTimer), and so is the
 *     longshot to the barred door; eca767dcb's hammer recoil hover crosses back
 *     from the barred door (and takes the heart piece from there) only with
 *     RT_HOVER_BOOST_SIMPLE, and its Bunny Hood hover takes the distant platform
 *     rupee with no Golden Gauntlets only under Bunny Hood Effect. Master Quest: the main
 *     room enters From Open Door, whose way to the barred door needs FireTimer 32
 *     (four hearts under the trick, no Goron Tunic test) and Longshot, or Hover
 *     Boots with a hit to take; its under-pillar rupee needs the Golden
 *     Gauntlets. A scene search from each main room reaches the
 *     trial's Final Room and clears the trial (the generation failure 88099e6eb
 *     fixed was a trial no search could finish).
 *
 * SoH eca767dcb's Bunny Hood Effect seed setting (RSK_BUNNY_HOOD) and SoH
 * e7f1801f0's Lost Woods bridge jump (#7488):
 *  BH. The one reader of the hood's effect in Hyrule (OoT_BunnyHoodEffectMode)
 *     forces Faster + Longer Jump only in a randomizer file whose seed has the
 *     setting on (not in a vanilla file while the rando context holds it on, not
 *     with it off), over a weaker enhancement, without writing the enhancement's
 *     CVar; its decision keeps the shared Bunny Hood rule's raise to Faster Run
 *     with the setting off (#969). Logic::BunnyHood is false with the setting off
 *     even for a child holding the hood, true with it on, and false for an adult.
 *  LW. RR_THE_LOST_WOODS -> RR_LW_BRIDGE: a child with the hood and a sword must
 *     NOT jump to the bridge with the setting off and must with it on; with the
 *     hood alone (no jumpslash) must NOT; an adult with the hood must NOT.
 *
 * The rest of SoH eca767dcb's live child Bunny Hood terms and its recoil-hover
 * helpers (#1647, stages 1 and 2), with SoH 73a739b7a's high-emblem trick:
 *  BL. One pair per dungeon: a child holding the hood must NOT pass with Bunny
 *     Hood Effect off and must with it on (Deku basement, DC MQ upper Lizalfos
 *     heart, Fire SoT cage, Forest block-push alcove and Ganon's Shadow Trial with
 *     a sword, GTG MQ platforms, Ice Cavern block room, Jabu MQ lift room under
 *     RT_UNINTUITIVE_JUMPS, Shadow MQ spikes, Spirit GS ledge, Water Like Like
 *     spikes, MQSpiritStatueToSunBlock with the bracelet, the Deku MQ compass-room
 *     boulder with Boomerang and bombs, the DC MQ lobby's lower-right mud wall with
 *     Goron's Bracelet and its bomb-flower ground jump, and Water's river pots to
 *     above the dragon with the slingshot).
 *  OW. The same pairs in Kakariko (backyard), the graveyard (Dampe's race), Death
 *     Mountain Trail (chest with Goron's Bracelet), Zora's Domain (island exit with
 *     Climb, closed without it) and Zora's River (ladder top to pillar); the Lake
 *     Hylia front rupee stays child-only with eca767dcb's Iron Boots term.
 *  HW. RT_HW_BUNNY_CROSSING crosses the wasteland only with the trick AND the
 *     setting on.
 *  WE. RT_WATER_HIGH_EMBLEM_JUMP: an adult with nothing reaches the high emblem
 *     (vanilla helper and MQ exit) only with the trick on; a child with the hood
 *     only with the trick and the setting; eca767dcb's Unintuitive Jumps form is
 *     not what credits the child.
 *  RH. CanRecoilHover / CanRecoilHoverFromObject are false without
 *     RT_HOVER_BOOST_SIMPLE; with it, RECOIL_HAMMER needs no shield and
 *     RECOIL_HAMMER_AND_SHIELD does; the old CanHammerRecoilHover() call sites
 *     (Forest courtyard, GF long roof, GV upper stream) open with Hover Boots and
 *     the hammer and no shield, and close with the trick off.
 *  D1. CanAvoidEnemy(RE_BEAMOS, ED_CLOSE, grounded, 2) is still false with only
 *     the Bow (its body is unchanged; the Beamos rewrite is stage 5).
 *
 * SoH eca767dcb's Masks as Adult seed setting (RSK_MASKS_AS_ADULT) and the adult
 * hood terms it makes live (#1647 stage 1b):
 *  MA. The one reader of Masks Equippable as Adult (OoT_AdultMasksEnabled) is on
 *     in a randomizer file whose seed has the setting on with the enhancement
 *     unset, without writing the enhancement's CVar; off in a vanilla file while
 *     the rando context holds it on and with it off; the enhancement alone still
 *     works; Timeless Equipment still lets adult Link wear masks.
 *  MC. CanUse(RG_BUNNY_HOOD) and the Skull Mask are adult only with the setting on;
 *     the Keaton Mask stays child-only (upstream gives the setting to three masks);
 *     with Masks as Adult on and Bunny Hood Effect off BunnyHood() is still false.
 *  BV. BunnyHovers() is true only for an adult with Hover Boots, the hood and both
 *     settings on.
 *  AH. One pair per area: an adult holding the hood (and what the term names) must
 *     NOT pass with Masks as Adult off and must with it on, Bunny Hood Effect on.
 *  TR. RT_DC_EYES_BUNNYHOVERS lights the Dodongo's eyes only with the trick on.
 *
 * Every "must NOT" leg is paired with a "must" leg on the same save plus one
 * thing, so a location that is never reachable (a vacuous probe) fails too.
 * A0 and B0 are the exception in form: they hold everything but the event, and
 * the sibling that proves the location reachable (A2, B2) sets the event and
 * drops the letter, which is the point of those two legs.
 * Each probe sets logic->CurrentRegionKey to its region, as ProcessRegion does.
 */

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <libultraship/bridge.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/enhancementTypes.h" // BunnyHoodMode, OoT_BunnyHoodEffectMode, OoT_AdultMasksEnabled (BH, MA legs)
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/settings.h"

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"

#ifdef RSBS_SINGLE_EXECUTABLE
#include "context.h"       // src/common: gComboCtx (the BP legs freeze the cross-game mask rules)
#include "foreign_items.h" // src/common: RSBS_XITEM_BLAST_MASK_OOT, Combo_ComboSettingsDefaults
#include "soh/Enhancements/randomizer/ForeignMaskOoT.h" // OoT_ForeignMask_AdultMayWear (the BW legs)
#endif

namespace Rando {
// entrance.cpp, which no header declares: the DE legs stamp the entrance table on the live graph.
void SetAllEntrancesData();
} // namespace Rando

namespace {

int sFailures = 0;

void GateFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[OOT-LOGIC-GATES] FAIL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    sFailures++;
}

const LocationAccess* FindLocation(Region* region, RandomizerCheck rc) {
    for (const LocationAccess& loc : region->locations) {
        if (loc.GetLocation() == rc) {
            return &loc;
        }
    }
    return nullptr;
}

/** A fresh logic save under the forced options, then nothing else: each leg
 *  grants exactly what it names. */
void ResetLogic() {
    logic->Reset(true);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CalculatingAvailableChecks = false;
    // Reset already withholds both under Shuffle Speak / Shuffle Grab ON; stated
    // anyway so a change to Reset's defaults cannot hand a leg the very thing it
    // is about.
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_HYLIAN, false);
    logic->SetRandoInf(RAND_INF_CAN_GRAB, false);
}

void GiveNut() {
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_HYLIAN, true);
}
void GiveGrab() {
    logic->SetRandoInf(RAND_INF_CAN_GRAB, true);
}
void GiveChildWallet() {
    logic->SetRandoInf(RAND_INF_HAS_WALLET, true);
}
void GiveZeldasLetter() {
    logic->SetRandoInf(RAND_INF_ZELDAS_LETTER, true);
}
void GiveFairyOcarina() {
    logic->SetInventory(ITEM_OCARINA_FAIRY, ITEM_OCARINA_FAIRY);
}
/** A child's weapon for CanJumpslash / CanKillEnemy(RE_GERUDO_WARRIOR). */
void GiveSticks() {
    logic->SetUpgrade(UPG_STICKS, 1);
    logic->Set(LOGIC_STICK_ACCESS, true);
}
/** One item, the way the search grants it (Item::ApplyEffect -> Logic::ApplyItemEffect). */
void Give(RandomizerGet rg) {
    Rando::StaticData::RetrieveItem(rg).ApplyEffect();
}

/**
 * The search's question for one location of one region, as the child or the
 * adult (day and night), with the region current as ProcessRegion makes it.
 * Returns -1 when the probe could not be made (region or location missing),
 * which the caller counts as a failure whatever it expected.
 */
int Reaches(RandomizerRegion regionKey, RandomizerCheck rc, bool adult) {
    Region* region = RegionTable(regionKey);
    const LocationAccess* loc = region != nullptr ? FindLocation(region, rc) : nullptr;
    if (loc == nullptr) {
        return -1;
    }
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    const RandomizerRegion savedRegionKey = logic->CurrentRegionKey;
    region->childDay = !adult;
    region->childNight = !adult;
    region->adultDay = adult;
    region->adultNight = adult;
    logic->CurrentRegionKey = regionKey;
    while (region->UpdateEvents()) {}
    logic->CurrentCheckKey = rc;
    const bool met = loc->ConditionsMet(region, false);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CurrentRegionKey = savedRegionKey;
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
    return met ? 1 : 0;
}

/**
 * One region's own events fired to a fixpoint for one age (day and night), the
 * way ProcessRegion fires them, with the region current (Logic::ItemUseAllowed
 * reads its scene). The events that fire stay set in logic for the caller to
 * read; the region's access is restored.
 */
void FireEvents(RandomizerRegion regionKey, bool adult) {
    Region* region = RegionTable(regionKey);
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    const RandomizerRegion savedRegionKey = logic->CurrentRegionKey;
    region->childDay = !adult;
    region->childNight = !adult;
    region->adultDay = adult;
    region->adultNight = adult;
    logic->CurrentRegionKey = regionKey;
    while (region->UpdateEvents()) {}
    logic->CurrentRegionKey = savedRegionKey;
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
}

void ExpectEvent(const char* leg, LogicVal event, bool expected, const char* why) {
    const bool got = logic->Get(event);
    if (got != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "set" : "unset", got ? "set" : "unset");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "set" : "unset");
}

/** The Weird Egg, the way the search grants it (RAND_INF_WEIRD_EGG). */
void GiveWeirdEgg() {
    Give(RG_WEIRD_EGG);
}

void ExpectAs(bool adult, const char* leg, RandomizerRegion regionKey, RandomizerCheck rc, bool expected,
              const char* why) {
    const int got = Reaches(regionKey, rc, adult);
    if (got < 0) {
        GateFail("%s: location %d is not in region %d, so this probe evaluates nothing", leg, (int)rc, (int)regionKey);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "reachable" : "unreachable",
                 got ? "reachable" : "unreachable");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "reachable" : "unreachable");
}

void Expect(const char* leg, RandomizerRegion regionKey, RandomizerCheck rc, bool expected, const char* why) {
    ExpectAs(false, leg, regionKey, rc, expected, why);
}

/** A pot whose scene disables B, the hookshot and every C item (leg F). */
struct HousePot {
    const char* name;
    RandomizerRegion region;
    RandomizerCheck check;
};

const HousePot kHousePots[] = {
    { "Link's House pot", RR_KF_LINKS_HOUSE, RC_KF_LINKS_HOUSE_POT },
    { "Twins' House pot 1", RR_KF_HOUSE_OF_TWINS, RC_KF_TWINS_HOUSE_POT_1 },
    { "Twins' House pot 2", RR_KF_HOUSE_OF_TWINS, RC_KF_TWINS_HOUSE_POT_2 },
    { "Know-It-All Brothers' pot 1", RR_KF_KNOW_IT_ALL_HOUSE, RC_KF_BROTHERS_HOUSE_POT_1 },
    { "Know-It-All Brothers' pot 2", RR_KF_KNOW_IT_ALL_HOUSE, RC_KF_BROTHERS_HOUSE_POT_2 },
    { "back-alley house pot 1", RR_MARKET_MAN_IN_GREEN_HOUSE, RC_MK_BACK_ALLEY_HOUSE_POT_1 },
    { "back-alley house pot 2", RR_MARKET_MAN_IN_GREEN_HOUSE, RC_MK_BACK_ALLEY_HOUSE_POT_2 },
    { "back-alley house pot 3", RR_MARKET_MAN_IN_GREEN_HOUSE, RC_MK_BACK_ALLEY_HOUSE_POT_3 },
};

/** Every child item CanBreakPots names: B (Kokiri Sword, sticks) and C (slingshot, boomerang, bombs). */
void GiveChildPotBreakers() {
    Give(RG_KOKIRI_SWORD);
    GiveSticks();
    Give(RG_PROGRESSIVE_SLINGSHOT);
    Give(RG_BOOMERANG);
    Give(RG_PROGRESSIVE_BOMB_BAG);
}

struct Carpenter {
    const char* name;
    RandomizerRegion region;
    RandomizerCheck check;
    /** The cell guard's En_GeldB params >> 8 in the scene's actor list: the
     *  collectible flag of the key it drops (leg E). */
    uint8_t guardKeyFlag;
};

const Carpenter kCarpenters[] = {
    { "1 Torch carpenter", RR_TH_1_TORCH_CELL, RC_TH_1_TORCH_CARPENTER, 0x0C },             // gerudoway_room_2
    { "Double Cell carpenter", RR_TH_DOUBLE_CELL, RC_TH_DOUBLE_CELL_CARPENTER, 0x0F },      // gerudoway_room_5
    { "Dead End carpenter", RR_TH_DEAD_END_CELL, RC_TH_DEAD_END_CARPENTER, 0x0A },          // gerudoway_room_1
    { "Steep Slope carpenter", RR_TH_STEEP_SLOPE_CELL, RC_TH_STEEP_SLOPE_CARPENTER, 0x0E }, // gerudoway_room_4
};

/** Leg E for one carpenter: its row carries the guard's drop and flag, and no
 *  other row answers that drop. */
void ExpectCarpenterRow(const Carpenter& carpenter) {
    const int32_t dropParams = ((int32_t)carpenter.guardKeyFlag << 8) | ITEM00_SMALL_KEY;
    const Rando::Location* row = Rando::StaticData::GetLocation(carpenter.check);
    const Rando::SpoilerCollectionCheck cc = row->GetCollectionCheck();
    bool ok = true;
    if (row->GetActorID() != ACTOR_EN_ITEM00 || row->GetScene() != SCENE_THIEVES_HIDEOUT) {
        GateFail("E %s: the row is actor %d in scene %d, not the guard's En_Item00 drop in the hideout", carpenter.name,
                 (int)row->GetActorID(), (int)row->GetScene());
        ok = false;
    }
    if (row->GetActorParams() != dropParams) {
        GateFail("E %s: the row's actor params are 0x%04X but the cell's guard drops 0x%04X (#1191): the "
                 "carpenter's key awards another cell's check",
                 carpenter.name, (unsigned)row->GetActorParams(), (unsigned)dropParams);
        ok = false;
    }
    if (cc.type != SPOILER_CHK_COLLECTABLE || cc.scene != SCENE_THIEVES_HIDEOUT || cc.flag != carpenter.guardKeyFlag) {
        GateFail("E %s: the row's collection check is type %d scene %d flag 0x%02X, not the cell guard's "
                 "collectible flag 0x%02X (#1191): the tracker marks another cell's check",
                 carpenter.name, (int)cc.type, (int)cc.scene, (unsigned)cc.flag, (unsigned)carpenter.guardKeyFlag);
        ok = false;
    }
    // The runtime lookup (Randomizer::GetCheckFromActor over the multimap
    // InitHashMaps builds from this table) must find this row and only it.
    int matches = 0;
    RandomizerCheck found = RC_UNKNOWN_CHECK;
    for (const Rando::Location& loc : Rando::StaticData::GetLocationTable()) {
        if (loc.GetActorID() == ACTOR_EN_ITEM00 && loc.GetScene() == SCENE_THIEVES_HIDEOUT &&
            loc.GetActorParams() == dropParams) {
            matches++;
            found = loc.GetRandomizerCheck();
        }
    }
    if (matches != 1 || found != carpenter.check) {
        GateFail("E %s: %d row(s) answer the guard's drop 0x%04X (last %d), expected exactly the carpenter's (%d)",
                 carpenter.name, matches, (unsigned)dropParams, (int)found, (int)carpenter.check);
        ok = false;
    }
    if (ok) {
        printf("[TEST]   ok: E %s: params 0x%04X, flag 0x%02X\n", carpenter.name, (unsigned)dropParams,
               (unsigned)carpenter.guardKeyFlag);
    }
}

// ---- #1277: Shuffle Climb legs (F..P) --------------------------------------
// The ladders SoH re-gated after our import base (bundle O5). Every probe runs
// the real lambdas RegionTable_Init built, under Shuffle Climb ON with
// RAND_INF_CAN_CLIMB withheld unless the leg grants it.

void GiveClimb() {
    logic->SetRandoInf(RAND_INF_CAN_CLIMB, true);
}
void GiveOpenChest() {
    logic->SetRandoInf(RAND_INF_CAN_OPEN_CHEST, true);
}
void GiveKokiriSword() {
    logic->ApplyItemEffect(Rando::StaticData::RetrieveItem(RG_KOKIRI_SWORD), true);
}
void GiveSlingshot() {
    logic->ApplyItemEffect(Rando::StaticData::RetrieveItem(RG_PROGRESSIVE_SLINGSHOT), true);
}
/** ITEM_HOOKSHOT's slot holding the hookshot or the longshot. */
void GiveHookshot(bool longshot) {
    logic->SetInventory(ITEM_HOOKSHOT, longshot ? ITEM_LONGSHOT : ITEM_HOOKSHOT);
}

/** ResetLogic, plus the Shuffle Climb / Shuffle Open Chest abilities withheld. */
void ResetClimbLogic() {
    ResetLogic();
    logic->SetRandoInf(RAND_INF_CAN_CLIMB, false);
    logic->SetRandoInf(RAND_INF_CAN_OPEN_CHEST, false);
}

bool AnyAccess(const Region* region) {
    return region->childDay || region->childNight || region->adultDay || region->adultNight;
}

/** One exit's own lambda, for one age, by day or by night. -1 when the region
 *  has no exit to `to` (the probe would evaluate nothing). */
int ExitOpen(RandomizerRegion from, RandomizerRegion to, bool adult) {
    if (from <= RR_NONE || from >= RR_MAX || to <= RR_NONE || to >= RR_MAX) {
        return -1;
    }
    for (Rando::Entrance& exit : RegionTable(from)->exits) {
        if (exit.GetConnectedRegionKey() != to) {
            continue;
        }
        logic->CurrentRegionKey = from;
        const bool day = exit.CheckConditionAtAgeTime(adult ? logic->IsAdult : logic->IsChild, logic->AtDay);
        const bool night = exit.CheckConditionAtAgeTime(adult ? logic->IsAdult : logic->IsChild, logic->AtNight);
        logic->CurrentRegionKey = RR_NONE;
        return (day || night) ? 1 : 0;
    }
    return -1;
}

void ExpectExit(const char* leg, RandomizerRegion from, RandomizerRegion to, bool adult, bool expected,
                const char* why) {
    const int got = ExitOpen(from, to, adult);
    if (got < 0) {
        GateFail("%s: region %d has no exit to region %d, so this probe evaluates nothing", leg, (int)from, (int)to);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "open" : "closed", got ? "open" : "closed");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "open" : "closed");
}

/** ChildReaches for either age (day and night). */
int AgeReaches(RandomizerRegion regionKey, RandomizerCheck rc, bool adult) {
    Region* region = RegionTable(regionKey);
    const LocationAccess* loc = region != nullptr ? FindLocation(region, rc) : nullptr;
    if (loc == nullptr) {
        return -1;
    }
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    region->childDay = !adult;
    region->childNight = !adult;
    region->adultDay = adult;
    region->adultNight = adult;
    logic->CurrentRegionKey = regionKey;
    while (region->UpdateEvents()) {}
    logic->CurrentCheckKey = rc;
    const bool met = loc->ConditionsMet(region, false);
    logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
    logic->CurrentRegionKey = RR_NONE;
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
    return met ? 1 : 0;
}

void ExpectAt(const char* leg, RandomizerRegion regionKey, RandomizerCheck rc, bool adult, bool expected,
              const char* why) {
    const int got = AgeReaches(regionKey, rc, adult);
    if (got < 0) {
        GateFail("%s: location %d is not in region %d, so this probe evaluates nothing", leg, (int)rc, (int)regionKey);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "reachable" : "unreachable",
                 got ? "reachable" : "unreachable");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "reachable" : "unreachable");
}

/**
 * A one-age search confined to the start region's scene: every region's access
 * is cleared, the start gets the age (day and night), and access spreads through
 * the exits' own lambdas while each reached region fires its events, to a
 * fixpoint (3drando/fill.cpp's ProcessRegion / ProcessExits, without time
 * travel). With rc == RC_UNKNOWN_CHECK it answers whether `targetRegion` was
 * reached; otherwise whether a reached region of the scene holding `rc` meets
 * its condition (-1 when no region of the scene holds `rc`). Access is restored.
 */
int SceneSearch(RandomizerRegion start, bool adult, RandomizerRegion targetRegion, RandomizerCheck rc) {
    std::vector<std::array<bool, 4>> saved(RR_MAX);
    for (int r = RR_NONE + 1; r < RR_MAX; r++) {
        Region* region = RegionTable((RandomizerRegion)r);
        saved[r] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
        region->childDay = region->childNight = region->adultDay = region->adultNight = false;
    }
    Region* first = RegionTable(start);
    const SceneID scene = first->scene;
    first->childDay = first->childNight = !adult;
    first->adultDay = first->adultNight = adult;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int r = RR_NONE + 1; r < RR_MAX; r++) {
            Region* region = RegionTable((RandomizerRegion)r);
            if (!AnyAccess(region)) {
                continue;
            }
            logic->CurrentRegionKey = (RandomizerRegion)r;
            while (region->UpdateEvents()) {
                changed = true;
            }
            for (Rando::Entrance& exit : region->exits) {
                if (exit.GetConnectedRegionKey() == RR_NONE) {
                    continue;
                }
                Region* to = exit.GetConnectedRegion();
                if (to->scene != scene) {
                    continue;
                }
                bool* const from[4] = { &region->childDay, &region->childNight, &region->adultDay,
                                        &region->adultNight };
                bool* const into[4] = { &to->childDay, &to->childNight, &to->adultDay, &to->adultNight };
                for (int i = 0; i < 4; i++) {
                    if (!*from[i] || *into[i]) {
                        continue;
                    }
                    if (exit.CheckConditionAtAgeTime(i < 2 ? logic->IsChild : logic->IsAdult,
                                                     (i % 2) == 0 ? logic->AtDay : logic->AtNight)) {
                        *into[i] = true;
                        changed = true;
                    }
                }
            }
        }
    }

    int result = 0;
    if (rc == RC_UNKNOWN_CHECK) {
        result = AnyAccess(RegionTable(targetRegion)) ? 1 : 0;
    } else {
        bool held = false;
        for (int r = RR_NONE + 1; r < RR_MAX && result == 0; r++) {
            Region* region = RegionTable((RandomizerRegion)r);
            const LocationAccess* loc = region->scene == scene ? FindLocation(region, rc) : nullptr;
            if (loc == nullptr) {
                continue;
            }
            held = true;
            if (!AnyAccess(region)) {
                continue;
            }
            logic->CurrentRegionKey = (RandomizerRegion)r;
            logic->CurrentCheckKey = rc;
            result = loc->ConditionsMet(region, false) ? 1 : 0;
            logic->CurrentCheckKey = RC_UNKNOWN_CHECK;
        }
        if (!held) {
            result = -1;
        }
    }
    logic->CurrentRegionKey = RR_NONE;

    for (int r = RR_NONE + 1; r < RR_MAX; r++) {
        Region* region = RegionTable((RandomizerRegion)r);
        region->childDay = saved[r][0];
        region->childNight = saved[r][1];
        region->adultDay = saved[r][2];
        region->adultNight = saved[r][3];
    }
    return result;
}

void ExpectSearch(const char* leg, RandomizerRegion start, bool adult, RandomizerRegion targetRegion,
                  RandomizerCheck rc, bool expected, const char* why) {
    const int got = SceneSearch(start, adult, targetRegion, rc);
    if (got < 0) {
        GateFail("%s: no region of the start's scene holds location %d, so this probe evaluates nothing", leg, (int)rc);
        return;
    }
    if ((got == 1) != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "reachable" : "unreachable",
                 got ? "reachable" : "unreachable");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, expected ? "reachable" : "unreachable");
}

/**
 * Leg F's question: does any exit of Kak Backyard other than its three
 * ground-level ones (the village, the open grotto, Granny's shop door) open for
 * an adult by day? Those others lead to the potion shop's back side, up the
 * backyard ladder. -1 when the backyard has no such exit.
 */
int KakBackyardLadderSideOpen() {
    int found = 0;
    int open = 0;
    for (Rando::Entrance& exit : RegionTable(RR_KAK_BACKYARD)->exits) {
        const RandomizerRegion to = exit.GetConnectedRegionKey();
        if (to == RR_KAKARIKO_VILLAGE || to == RR_KAK_OPEN_GROTTO || to == RR_KAK_ODD_POTION_BUILDING ||
            to == RR_NONE) {
            continue;
        }
        found++;
        logic->CurrentRegionKey = RR_KAK_BACKYARD;
        if (exit.CheckConditionAtAgeTime(logic->IsAdult, logic->AtDay)) {
            open++;
        }
        logic->CurrentRegionKey = RR_NONE;
    }
    return found == 0 ? -1 : (open > 0 ? 1 : 0);
}

bool RegionHasExitTo(RandomizerRegion from, RandomizerRegion to) {
    for (Rando::Entrance& exit : RegionTable(from)->exits) {
        if (exit.GetConnectedRegionKey() == to) {
            return true;
        }
    }
    return false;
}

void RunShuffleClimbLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedShuffleClimb = randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Get();
    const uint8_t savedShuffleOpenChest = randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Get();
    const uint8_t savedUnintuitive = randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Get();
    const uint8_t savedHoverBoost = randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Get();
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(1);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(1);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);

    // ---- F: the Kak backyard ladder to the potion shop's back (SoH b6cdc6c49)
    ResetClimbLogic();
    const int f1 = KakBackyardLadderSideOpen();
    if (f1 < 0) {
        GateFail("F1 Kak backyard: no exit beyond the ground-level three, so the probe evaluates nothing");
    } else if (f1 != 0) {
        GateFail("F1 Kak backyard, adult by day, no Climb: the potion shop's back side opens from the backyard "
                 "without climbing its ladder (#1277)");
    } else {
        printf("[TEST]   ok: F1 Kak backyard ladder, adult by day, no Climb: closed\n");
    }
    GiveClimb();
    if (KakBackyardLadderSideOpen() != 1) {
        GateFail("F2 Kak backyard, adult by day, with Climb: the potion shop's back side stays closed");
    } else {
        printf("[TEST]   ok: F2 Kak backyard ladder, adult by day, with Climb: open\n");
    }
    // The watchtower jump to the rooftop is any age under the trick (same commit).
    ResetClimbLogic();
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(1);
    ExpectExit("F3 Kak watchtower to rooftop, adult, unintuitive jumps", RR_KAK_WATCHTOWER, RR_KAK_ROOFTOP, true, true,
               "the trick's watchtower jump to the rooftop is child-only");
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    ExpectExit("F4 Kak watchtower to rooftop, adult, no trick", RR_KAK_WATCHTOWER, RR_KAK_ROOFTOP, true, false,
               "the watchtower jump is in logic without the trick");

    // ---- G: the DMC wall Piece of Heart needs Climb (SoH ec27775e8, PoH clause)
    ResetClimbLogic();
    if (logic->Health() < 3 * 16) {
        GateFail("G: Logic::Reset left health %d, so the heat term fails and the legs are vacuous",
                 (int)logic->Health());
    }
    ExpectAt("G1 DMC wall PoH, adult with 3 hearts, no Climb", RR_DMC_UPPER_ENTRY, RC_DMC_WALL_FREESTANDING_POH, true,
             false, "the wall alcove is in logic without climbing down to it (#1277)");
    GiveClimb();
    ExpectAt("G2 DMC wall PoH, adult with 3 hearts and Climb", RR_DMC_UPPER_ENTRY, RC_DMC_WALL_FREESTANDING_POH, true,
             true, "the wall alcove is out of logic with Climb and the hearts");

    // ---- H: DC Armos Room to Bomb Room Lower (SoH fe5dce24b) ---------------
    ResetClimbLogic();
    GiveGrab();
    ExpectExit("H1 DC Armos Room to Bomb Room Lower, child with bracelet, no Climb", RR_DODONGOS_CAVERN_ARMOS_ROOM,
               RR_DODONGOS_CAVERN_BOMB_ROOM_LOWER, false, false,
               "a child with the bracelet passes the ladder without Climb (#1277)");
    GiveClimb();
    ExpectExit("H2 DC Armos Room to Bomb Room Lower, child with bracelet and Climb", RR_DODONGOS_CAVERN_ARMOS_ROOM,
               RR_DODONGOS_CAVERN_BOMB_ROOM_LOWER, false, true, "a child with bracelet and Climb cannot pass");

    // ---- I: the Child Spirit locked door leads to Sun On Floor 1F (SoH fe5dce24b)
    if (RegionHasExitTo(RR_SPIRIT_TEMPLE_CHILD_BOXES, RR_SPIRIT_TEMPLE_SUN_ON_FLOOR_2F) ||
        !RegionHasExitTo(RR_SPIRIT_TEMPLE_CHILD_BOXES, RR_SPIRIT_TEMPLE_SUN_ON_FLOOR_1F)) {
        GateFail("I Child Spirit before climb: the locked door leads to Sun On Floor 2F, past the climb, instead of "
                 "1F (#1277)");
    } else {
        printf("[TEST]   ok: I Child Spirit before climb: the locked door leads to Sun On Floor 1F\n");
    }

    // ---- J: the Deku Tree 2F grass is on 2F (SoH fe5dce24b) ----------------
    // The two grass checks after LOBBY_GRASS_3 (the fix renames them 2F_GRASS_1/2;
    // the enum values do not move).
    for (int i = 1; i <= 2; i++) {
        const RandomizerCheck grass = (RandomizerCheck)(RC_DEKU_TREE_LOBBY_GRASS_3 + i);
        char leg[96];
        ResetClimbLogic();
        GiveKokiriSword();
        snprintf(leg, sizeof(leg), "J%d Deku Tree 2F grass %d, child with a sword, no Climb", i * 2 - 1, i);
        ExpectSearch(leg, RR_DEKU_TREE_LOBBY, false, RR_NONE, grass, false,
                     "grass on the 2F ledge is cut from the lobby floor without climbing (#1277)");
        GiveClimb();
        snprintf(leg, sizeof(leg), "J%d Deku Tree 2F grass %d, child with a sword and Climb", i * 2, i);
        ExpectSearch(leg, RR_DEKU_TREE_LOBBY, false, RR_NONE, grass, true, "the 2F grass is out of reach with Climb");
    }

    // ---- K: the Deku Tree map chest is on 2F and needs Open Chest (SoH 743103fbe)
    ResetClimbLogic();
    GiveOpenChest();
    ExpectSearch("K1 Deku Tree map chest, child with Open Chest, no Climb", RR_DEKU_TREE_LOBBY, false, RR_NONE,
                 RC_DEKU_TREE_MAP_CHEST, false, "the 2F map chest is listed on the lobby floor (#1277)");
    ResetClimbLogic();
    GiveClimb();
    ExpectSearch("K2 Deku Tree map chest, child with Climb, no Open Chest", RR_DEKU_TREE_LOBBY, false, RR_NONE,
                 RC_DEKU_TREE_MAP_CHEST, false, "the 2F map chest ignores Shuffle Open Chest (#1277)");
    GiveOpenChest();
    ExpectSearch("K3 Deku Tree map chest, child with Climb and Open Chest", RR_DEKU_TREE_LOBBY, false, RR_NONE,
                 RC_DEKU_TREE_MAP_CHEST, true, "the map chest is out of reach with Climb and Open Chest");

    // ---- L: the GV waterfall alcove needs the Longshot AND Climb (SoH 09b0fdbc9,
    // without its RT_HOOKSHOT_LADDERS clause) -------------------------------------
    ResetClimbLogic();
    GiveHookshot(true);
    ExpectExit("L1 GV waterfall alcove, adult with Longshot, no Climb", RR_GV_UPPER_STREAM, RR_GV_WATERFALL_ALCOVE,
               true, false, "the Longshot alone reaches the alcove up the ladder (#1277)");
    ResetClimbLogic();
    GiveHookshot(false);
    GiveClimb();
    ExpectExit("L2 GV waterfall alcove, adult with Hookshot and Climb", RR_GV_UPPER_STREAM, RR_GV_WATERFALL_ALCOVE,
               true, false, "the Hookshot cannot reach the alcove's ladder, but logic says it can (#1277)");
    ResetClimbLogic();
    GiveHookshot(true);
    GiveClimb();
    ExpectExit("L3 GV waterfall alcove, adult with Longshot and Climb", RR_GV_UPPER_STREAM, RR_GV_WATERFALL_ALCOVE,
               true, true, "the Longshot and Climb cannot reach the alcove");

    // ---- M: a child leaves SFM only through the gate it opened (SoH ed4868f62)
    ResetClimbLogic();
    ExpectExit("M1 SFM to entryway, child who never killed the Wolfos", RR_SACRED_FOREST_MEADOW, RR_SFM_ENTRYWAY, false,
               false, "a child walks back through the Wolfos gate it never opened (#1277)");
    GiveKokiriSword();
    if (SceneSearch(RR_SFM_ENTRYWAY, false, RR_SACRED_FOREST_MEADOW, RC_UNKNOWN_CHECK) != 1) {
        GateFail("M2 setup: a child with the Kokiri Sword does not reach the meadow from the entryway");
    }
    // The search fired the entryway's events, and they stay set in logic.
    ExpectExit("M2 SFM to entryway, child who killed the Wolfos", RR_SACRED_FOREST_MEADOW, RR_SFM_ENTRYWAY, false, true,
               "the opened Wolfos gate does not let the child back out");
    ResetClimbLogic();
    ExpectExit("M3 SFM to entryway, adult", RR_SACRED_FOREST_MEADOW, RR_SFM_ENTRYWAY, true, true,
               "an adult cannot walk out of the meadow");

    // ---- N: the MQ DC torch room's ladder to the entrance-side bridge (SoH
    // 848106039, CanClimbLadder read as HasItem(RG_CLIMB): RT_HOOKSHOT_LADDERS is
    // not ported) ------------------------------------------------------------
    ResetClimbLogic();
    ExpectSearch("N1 MQ DC torch room, child with nothing", RR_DODONGOS_CAVERN_MQ_TORCH_PUZZLE_LOWER, false,
                 RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_UNKNOWN_CHECK, false,
                 "the Dodongo room is reached from the torch room floor without climbing (#1277)");
    GiveClimb();
    ExpectSearch("N2 MQ DC torch room, child with Climb", RR_DODONGOS_CAVERN_MQ_TORCH_PUZZLE_LOWER, false,
                 RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_UNKNOWN_CHECK, true,
                 "a child with Climb cannot reach the Dodongo room from the torch room");
    ResetClimbLogic();
    GiveGrab();
    ExpectSearch("N3 MQ DC torch room, child with the bracelet", RR_DODONGOS_CAVERN_MQ_TORCH_PUZZLE_LOWER, false,
                 RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_UNKNOWN_CHECK, true,
                 "a child with the bracelet cannot reach the Dodongo room from the torch room");

    // ---- O: the Jabu MQ near-boss chest needs Open Chest (SoH 7be5a4fea) ---
    ResetClimbLogic();
    GiveSlingshot();
    ExpectAt("O1 Jabu MQ near-boss chest, child with slingshot, no Open Chest", RR_JABU_JABUS_BELLY_MQ_NEAR_BOSS_ROOM,
             RC_JABU_JABUS_BELLY_MQ_NEAR_BOSS_CHEST, false, false, "the chest ignores Shuffle Open Chest (#1277)");
    GiveOpenChest();
    ExpectAt("O2 Jabu MQ near-boss chest, child with slingshot and Open Chest", RR_JABU_JABUS_BELLY_MQ_NEAR_BOSS_ROOM,
             RC_JABU_JABUS_BELLY_MQ_NEAR_BOSS_CHEST, false, true, "the chest is out of logic with the slingshot");

    // ---- P: the MQ Spirit child doors under hover boosts (SoH 688913270) ---
    // With the hover-boost trick a child can recoil past the jet and spend a key
    // without Statue Room access, so each door needs one key more.
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(1);
    ResetClimbLogic();
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 1);
    ExpectExit("P1 MQ Spirit to Under Like Like, child, 1 key, hover boosts", RR_SPIRIT_TEMPLE_MQ_1F_CHEST_SWITCH,
               RR_SPIRIT_TEMPLE_MQ_UNDER_LIKE_LIKE, false, false,
               "one key opens the door though a hover boost can waste it (#1277)");
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 2);
    ExpectExit("P2 MQ Spirit to Under Like Like, child, 2 keys, hover boosts", RR_SPIRIT_TEMPLE_MQ_1F_CHEST_SWITCH,
               RR_SPIRIT_TEMPLE_MQ_UNDER_LIKE_LIKE, false, true, "two keys do not open the door");
    ExpectExit("P3 MQ Spirit to Statue Room Child, child, 2 keys, hover boosts", RR_SPIRIT_TEMPLE_MQ_SUN_ON_FLOOR,
               RR_SPIRIT_TEMPLE_MQ_STATUE_ROOM_CHILD, false, false,
               "two keys open the statue room door though a hover boost can waste one (#1277)");
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 3);
    ExpectExit("P4 MQ Spirit to Statue Room Child, child, 3 keys, hover boosts", RR_SPIRIT_TEMPLE_MQ_SUN_ON_FLOOR,
               RR_SPIRIT_TEMPLE_MQ_STATUE_ROOM_CHILD, false, true, "three keys do not open the door");
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);
    ResetClimbLogic();
    logic->SetSmallKeyCount(SCENE_SPIRIT_TEMPLE, 1);
    ExpectExit("P5 MQ Spirit to Under Like Like, child, 1 key, no hover boosts", RR_SPIRIT_TEMPLE_MQ_1F_CHEST_SWITCH,
               RR_SPIRIT_TEMPLE_MQ_UNDER_LIKE_LIKE, false, true, "without the trick one key no longer opens the door");

    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(savedShuffleClimb);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(savedShuffleOpenChest);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(savedUnintuitive);
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(savedHoverBoost);
}

void GiveZeldasLullaby() {
    GiveFairyOcarina();
    Give(RG_ZELDAS_LULLABY);
}

void GiveGoronsBracelet() {
    Give(RG_PROGRESSIVE_STRENGTH);
    Give(RG_PROGRESSIVE_STRENGTH);
}

// Appended by #1300 right after RR_DODONGOS_CAVERN_MQ_ENTRANCE_SIDE_BRIDGE and
// RT_GANONS_CASTLE_GOLD_GAUNTLET_SKIP (and RT_GF_WASTELAND_GATE_SIDEHOP_SKIP
// renames RT_GF_CHILD_SKIP_WASTELAND_GATE in place); guarded against RR_MAX /
// RT_MAX so the [lock] commit is test-only and compiles on the pre-fix tree.
constexpr RandomizerRegion kLwUnderBridge =
    static_cast<RandomizerRegion>(RR_DODONGOS_CAVERN_MQ_ENTRANCE_SIDE_BRIDGE + 1);
constexpr RandomizerRegion kGfTower = static_cast<RandomizerRegion>(RR_DODONGOS_CAVERN_MQ_ENTRANCE_SIDE_BRIDGE + 2);
constexpr RandomizerTrick kHookshotLaddersTrick = static_cast<RandomizerTrick>(RT_GANONS_CASTLE_GOLD_GAUNTLET_SKIP + 1);
constexpr RandomizerTrick kGfWastelandGateSidehopSkip = static_cast<RandomizerTrick>(RT_PASS_GUARDS_WITH_NOTHING + 1);
static_assert(kLwUnderBridge == RR_LW_UNDER_BRIDGE, "kLwUnderBridge drifted from RR_LW_UNDER_BRIDGE");
static_assert(kGfTower == RR_GF_TOWER, "kGfTower drifted from RR_GF_TOWER");
static_assert(kHookshotLaddersTrick == RT_HOOKSHOT_LADDERS, "kHookshotLaddersTrick drifted from RT_HOOKSHOT_LADDERS");
static_assert(kGfWastelandGateSidehopSkip == RT_GF_WASTELAND_GATE_SIDEHOP_SKIP,
              "kGfWastelandGateSidehopSkip drifted from RT_GF_WASTELAND_GATE_SIDEHOP_SKIP");

uint8_t GetHookshotLadders(const std::shared_ptr<Rando::Context>& randoCtx) {
    return kHookshotLaddersTrick < RT_MAX ? randoCtx->GetTrickOption(kHookshotLaddersTrick).Get() : 0;
}

void SetHookshotLadders(const std::shared_ptr<Rando::Context>& randoCtx, uint8_t val) {
    if (kHookshotLaddersTrick < RT_MAX) {
        randoCtx->GetTrickOption(kHookshotLaddersTrick).Set(val);
    }
}

void RunHookshotLaddersLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedShuffleClimb = randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Get();
    const uint8_t savedShuffleOpenChest = randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Get();
    const uint8_t savedHookshotLadders = GetHookshotLadders(randoCtx);
    const uint8_t savedUnintuitive = randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Get();
    const uint8_t savedGateSkip = randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Get();
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(1);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(1);
    SetHookshotLadders(randoCtx, 0);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(0);

    // ---- Q: GV waterfall alcove with RT_HOOKSHOT_LADDERS (SoH 09b0fdbc9) ---
    SetHookshotLadders(randoCtx, 1);
    ResetClimbLogic();
    GiveHookshot(false);
    ExpectExit("Q1 GV waterfall alcove, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_GV_UPPER_STREAM,
               RR_GV_WATERFALL_ALCOVE, true, false,
               "the Hookshot reaches the waterfall alcove though only the Longshot can (#1300)");
    GiveHookshot(true);
    ExpectExit("Q2 GV waterfall alcove, adult with Longshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_GV_UPPER_STREAM,
               RR_GV_WATERFALL_ALCOVE, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not reach the waterfall alcove (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- R: Dodongo's Cavern MQ compass chest (SoH 60ec65592) --------------
    ResetClimbLogic();
    GiveGoronsBracelet();
    GiveOpenChest();
    ExpectAt("R1 DC MQ compass chest, adult with Goron's Bracelet, no weapon, no Climb",
             RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, false,
             "pushing the block reaches the MQ compass chest without climbing the ladder (#1300)");
    GiveHookshot(false);
    ExpectAt("R2 DC MQ compass chest, adult with Goron's Bracelet and Hookshot, trick off, no Climb",
             RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, false,
             "the Hookshot climbs the MQ compass chest ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectAt("R3 DC MQ compass chest, adult with Goron's Bracelet and Hookshot, RT_HOOKSHOT_LADDERS on",
             RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM, RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, true,
             "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the MQ compass chest ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);
    GiveClimb();
    ExpectAt("R4 DC MQ compass chest, adult with Goron's Bracelet and Climb", RR_DODONGOS_CAVERN_MQ_DODONGO_ROOM,
             RC_DODONGOS_CAVERN_MQ_COMPASS_CHEST, true, true,
             "Goron's Bracelet and Climb cannot reach the MQ compass chest");

    // ---- S: Bottom of the Well ladders (SoH 60ec65592) ---------------------
    ResetClimbLogic();
    GiveHookshot(true);
    ExpectExit("S1 BotW B3 ooze to the ladder door, adult with Longshot, trick off, no Climb", RR_BOTW_B3_OOZE,
               RR_BOTW_LADDER_DOOR, true, false,
               "the Longshot climbs the BotW basement high ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit("S2 BotW MQ B3 to MQ perimeter, adult with Longshot, trick off, no Climb", RR_BOTW_MQ_B3,
               RR_BOTW_MQ_PERIMETER, true, false,
               "the Longshot climbs the BotW MQ basement high ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    GiveHookshot(false);
    ExpectExit("S3 BotW B3 ooze to the ladder door, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_BOTW_B3_OOZE, RR_BOTW_LADDER_DOOR, true, false,
               "the short Hookshot climbs the BotW basement high ladder (#1300)");
    GiveHookshot(true);
    ExpectExit("S4 BotW B3 ooze to the ladder door, adult with Longshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_BOTW_B3_OOZE, RR_BOTW_LADDER_DOOR, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not climb the BotW basement high ladder (#1300)");
    ExpectExit("S5 BotW MQ B3 to MQ perimeter, adult with Longshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_BOTW_MQ_B3,
               RR_BOTW_MQ_PERIMETER, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not climb the BotW MQ basement high ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- T: Shadow Temple boat dock ladder (SoH 60ec65592) -----------------
    ResetClimbLogic();
    GiveZeldasLullaby();
    GiveHookshot(false);
    ExpectExit("T1 Shadow dock to beyond boat, adult with Lullaby and Hookshot, trick off, no Climb",
               RR_SHADOW_TEMPLE_DOCK, RR_SHADOW_TEMPLE_BEYOND_BOAT, true, false,
               "the Hookshot climbs the boat dock ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("T2 Shadow dock to beyond boat, adult with Lullaby and Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_SHADOW_TEMPLE_DOCK, RR_SHADOW_TEMPLE_BEYOND_BOAT, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the boat dock ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);
    ResetClimbLogic();
    GiveZeldasLullaby();
    GiveGoronsBracelet();
    GiveClimb();
    ExpectExit("T3 Shadow dock to beyond boat, adult with Lullaby, Goron's Bracelet and Climb", RR_SHADOW_TEMPLE_DOCK,
               RR_SHADOW_TEMPLE_BEYOND_BOAT, true, true,
               "Goron's Bracelet and Climb do not reach the Shadow Temple boat from the dock");

    // ---- U: DMC ladder and Hyrule Field to Lake Hylia fence (SoH 60ec65592)
    // The crater ladder is 7522f31cc's blocked entry up to the crate ledge; the
    // Goron Tunic carries its heat term, so the ladder alone decides.
    ResetClimbLogic();
    GiveHookshot(false);
    Give(RG_GORON_TUNIC);
    ExpectExit("U1 DMC blocked entry to the crate ledge, adult with Hookshot, trick off, no Climb",
               RR_DMC_BLOCKED_ENTRY, RR_DMC_CRATE, true, false,
               "the Hookshot climbs the DMC ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit("U2 Hyrule Field to Lake Hylia, adult with Hookshot, trick off, no Climb", RR_HYRULE_FIELD,
               RR_HF_TO_LAKE_HYLIA, true, false,
               "the Hookshot climbs the Lake Hylia fence ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit(
        "U3 HF to Lake Hylia back to Hyrule Field, adult with Hookshot, trick off, no Climb", RR_HF_TO_LAKE_HYLIA,
        RR_HYRULE_FIELD, true, false,
        "the Hookshot climbs the Lake Hylia fence ladder from the lake side with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("U4 DMC blocked entry to the crate ledge, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_DMC_BLOCKED_ENTRY, RR_DMC_CRATE, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the DMC ladder (#1300)");
    ExpectExit("U5 Hyrule Field to Lake Hylia, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_HYRULE_FIELD,
               RR_HF_TO_LAKE_HYLIA, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Lake Hylia fence ladder (#1300)");
    ExpectExit(
        "U6 HF to Lake Hylia back to Hyrule Field, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
        RR_HF_TO_LAKE_HYLIA, RR_HYRULE_FIELD, true, true,
        "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Lake Hylia fence ladder from the lake side (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- V: Lost Woods under bridge and Sacred Forest Meadow (SoH 60ec65592)
    ResetClimbLogic();
    ExpectExit("V1 LW Under Bridge to Lost Woods, child with nothing", kLwUnderBridge, RR_THE_LOST_WOODS, false, false,
               "a child climbs from under the Lost Woods bridge without Climb");
    GiveHookshot(false);
    ExpectExit("V2 LW Under Bridge to Lost Woods, adult with Hookshot, trick off, no Climb", kLwUnderBridge,
               RR_THE_LOST_WOODS, true, false,
               "the Hookshot climbs the Lost Woods under-bridge ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("V3 LW Under Bridge to Lost Woods, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               kLwUnderBridge, RR_THE_LOST_WOODS, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Lost Woods under-bridge ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);
    GiveClimb();
    ExpectExit("V4 LW Under Bridge to Lost Woods, child with Climb", kLwUnderBridge, RR_THE_LOST_WOODS, false, true,
               "Climb does not reach Lost Woods from under the bridge (#1300)");

    ResetClimbLogic();
    GiveHookshot(false);
    ExpectExit("V5 SFM to Above Maze, adult with Hookshot, trick off, no Climb", RR_SACRED_FOREST_MEADOW,
               RR_SFM_ABOVE_MAZE, true, false,
               "the Hookshot climbs the SFM maze ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    ExpectExit("V6 SFM Outside Fairy Grotto to Above Maze, adult with Hookshot, trick off, no Climb",
               RR_SFM_OUTSIDE_FAIRY_GROTTO, RR_SFM_ABOVE_MAZE, true, false,
               "the Hookshot climbs the SFM fairy grotto ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("V7 SFM to Above Maze, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_SACRED_FOREST_MEADOW,
               RR_SFM_ABOVE_MAZE, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the SFM maze ladder (#1300)");
    ExpectExit("V8 SFM Outside Fairy Grotto to Above Maze, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_SFM_OUTSIDE_FAIRY_GROTTO, RR_SFM_ABOVE_MAZE, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the SFM fairy grotto ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);

    // ---- W: Gerudo Fortress tower and wasteland gate skip (SoH 60ec65592) --
    ResetClimbLogic();
    Give(RG_GERUDO_MEMBERSHIP_CARD);
    GiveHookshot(true);
    ExpectExit("W1 GF Outskirts to Tower, adult with Card and Longshot, trick off, no Climb", RR_GF_OUTSKIRTS, kGfTower,
               true, false, "the Longshot climbs the GF tower high ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    GiveHookshot(false);
    ExpectExit("W2 GF Outskirts to Tower, adult with Card and Hookshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_GF_OUTSKIRTS, kGfTower, true, false, "the short Hookshot climbs the GF tower high ladder (#1300)");
    GiveHookshot(true);
    ExpectExit("W3 GF Outskirts to Tower, adult with Card and Longshot, RT_HOOKSHOT_LADDERS on, no Climb",
               RR_GF_OUTSKIRTS, kGfTower, true, true,
               "the Longshot with RT_HOOKSHOT_LADDERS does not climb the GF tower high ladder (#1300)");
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_GERUDO, true);
    SetHookshotLadders(randoCtx, 0);
    ExpectSearch("W4 GF Outskirts to Outside Gate, adult with Longshot, Card and Speak, trick off, no Climb",
                 RR_GF_OUTSKIRTS, true, RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, false,
                 "LOGIC_GF_GATE_OPEN fires without climbing the GF tower (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectSearch("W5 GF Outskirts to Outside Gate, adult with Longshot, Card and Speak, RT_HOOKSHOT_LADDERS on",
                 RR_GF_OUTSKIRTS, true, RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, true,
                 "an adult on the GF tower with Card and Speak does not open the wasteland gate (#1300)");
    SetHookshotLadders(randoCtx, 0);

    ResetClimbLogic();
    GiveClimb();
    ExpectSearch("W6 GF Outskirts to Outside Gate, child with Climb, gate skip off", RR_GF_OUTSKIRTS, false,
                 RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, false,
                 "a child passes the wasteland gate without the sidehop skip trick");
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(1);
    ExpectSearch("W7 GF Outskirts to Outside Gate, child with Climb, RT_GF_WASTELAND_GATE_SIDEHOP_SKIP on",
                 RR_GF_OUTSKIRTS, false, RR_GF_OUTSIDE_GATE, RC_UNKNOWN_CHECK, true,
                 "the wasteland gate sidehop skip stays closed for a child (IsAdult && IsChild typo, #1300)");
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(0);

    // ---- X: Zora's River ladder (SoH 60ec65592) ----------------------------
    ResetClimbLogic();
    GiveHookshot(false);
    ExpectExit("X1 Zora's River to Atop Ladder, adult with Hookshot, trick off, no Climb", RR_ZORAS_RIVER,
               RR_ZR_ATOP_LADDER, true, false,
               "the Hookshot climbs the Zora's River ladder with RT_HOOKSHOT_LADDERS off (#1300)");
    SetHookshotLadders(randoCtx, 1);
    ExpectExit("X2 Zora's River to Atop Ladder, adult with Hookshot, RT_HOOKSHOT_LADDERS on, no Climb", RR_ZORAS_RIVER,
               RR_ZR_ATOP_LADDER, true, true,
               "the Hookshot with RT_HOOKSHOT_LADDERS does not climb the Zora's River ladder (#1300)");
    SetHookshotLadders(randoCtx, 0);

    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(savedShuffleClimb);
    randoCtx->GetOption(RSK_SHUFFLE_OPEN_CHEST).Set(savedShuffleOpenChest);
    SetHookshotLadders(randoCtx, savedHookshotLadders);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(savedUnintuitive);
    randoCtx->GetTrickOption(kGfWastelandGateSidehopSkip).Set(savedGateSkip);
}

// ---- #1397: health in 16-per-heart units and the DMT rockfall (Y..Z) ---------
void RunHealthLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedHearts = randoCtx->GetOption(RSK_STARTING_HEARTS).Get();
    const uint8_t savedDamage = randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Get();
    const uint8_t savedShieldless = randoCtx->GetTrickOption(RT_DMT_SHIELDLESS_CLIMB).Get();
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_DEFAULT);
    randoCtx->GetTrickOption(RT_DMT_SHIELDLESS_CLIMB).Set(0);

    // ---- Y: Logic::Reset seeds Starting Hearts; EffectiveHealth is 16/heart (SoH b52c31f85)
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(0); // one heart
    ResetLogic();
    if (logic->Health() != 16) {
        GateFail("Y1: Logic::Reset left health capacity %d under one Starting Heart, not 16", (int)logic->Health());
    }
    ExpectExit("Y1 GC grotto platform to Goron City, child, one heart, x1", RR_GC_GROTTO_PLATFORM, RR_GORON_CITY, false,
               false, "one heart survives the lava dive that costs a heart (a half-heart threshold)");
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(1); // two hearts
    ResetLogic();
    ExpectExit("Y2 GC grotto platform to Goron City, child, two hearts, x1", RR_GC_GROTTO_PLATFORM, RR_GORON_CITY,
               false, true, "two hearts do not survive the lava dive that costs one");

    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(2); // three hearts
    ResetLogic();
    const int plain = (int)logic->EffectiveHealth();
    Give(RG_DOUBLE_DEFENSE);
    const int doubled = (int)logic->EffectiveHealth();
    if (plain != 48 || doubled != 96) {
        GateFail("Y3: three hearts at x1 are EffectiveHealth %d, %d with Double Defense (expected 48, 96): "
                 "Double Defense must double what a hit chews through, never shrink it",
                 plain, doubled);
    } else {
        printf("[TEST]   ok: Y3 three hearts at x1: EffectiveHealth 48, 96 with Double Defense\n");
    }
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_OHKO);
    if (logic->EffectiveHealth() != 1) {
        GateFail("Y4: OHKO with Double Defense is EffectiveHealth %d, not 1", (int)logic->EffectiveHealth());
    } else {
        printf("[TEST]   ok: Y4 OHKO with Double Defense: EffectiveHealth 1\n");
    }
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_DEFAULT);

    // ---- Z: the rockfall needs adult, a shield, Nayru's Love or the trick (SoH 91425a39a)
    ResetLogic();
    GiveClimb();
    ExpectExit("Z1 DMT rockfall to summit, child with Climb, no shield", RR_DEATH_MOUNTAIN_ROCKFALL,
               RR_DEATH_MOUNTAIN_SUMMIT, false, false,
               "a child climbs past the eruption with no shield, Nayru's Love or RT_DMT_SHIELDLESS_CLIMB");
    ExpectExit("Z2 DMT rockfall to summit, adult with Climb", RR_DEATH_MOUNTAIN_ROCKFALL, RR_DEATH_MOUNTAIN_SUMMIT,
               true, true, "an adult with Climb cannot reach the summit");
    randoCtx->GetTrickOption(RT_DMT_SHIELDLESS_CLIMB).Set(1);
    ExpectExit("Z3 DMT rockfall to summit, child with Climb, RT_DMT_SHIELDLESS_CLIMB on", RR_DEATH_MOUNTAIN_ROCKFALL,
               RR_DEATH_MOUNTAIN_SUMMIT, false, true, "the shieldless-climb trick does not let a child past");

    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(savedHearts);
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(savedDamage);
    randoCtx->GetTrickOption(RT_DMT_SHIELDLESS_CLIMB).Set(savedShieldless);
}

// ---- #1647 (SoH e86e0ff69): VC, Voidout Collection ---------------------------
/** ResetLogic with Swim withheld, so the trick is the only way across the water. */
void VcResetNoSwim() {
    ResetLogic();
    logic->SetRandoInf(RAND_INF_CAN_SWIM, false);
}

/** One location for one age: out of logic with Voidout Collection off, in logic with it on. */
void VcCheckOffOn(const std::shared_ptr<Rando::Context>& randoCtx, const char* legOff, const char* legOn,
                  RandomizerRegion region, RandomizerCheck rc, bool adult) {
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);
    ExpectAt(legOff, region, rc, adult, false, "the voidout collection is in logic with RT_VOIDOUT_COLLECTION off");
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(1);
    ExpectAt(legOn, region, rc, adult, true,
             "the voidout collection is not in logic with RT_VOIDOUT_COLLECTION on (SoH e86e0ff69)");
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);
}

void RunVoidoutCollectionLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedVoidout = randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Get();
    const uint8_t savedDamage = randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Get();
    const uint8_t savedDetonation = randoCtx->GetTrickOption(RT_BOMB_DETONATION).Get();
    const uint8_t savedLensShadowMq = randoCtx->GetTrickOption(RT_LENS_SHADOW_MQ).Get();
    const uint8_t savedBunnyHood = randoCtx->GetOption(RSK_BUNNY_HOOD).Get();
    const uint8_t savedMasksAsAdult = randoCtx->GetOption(RSK_MASKS_AS_ADULT).Get();
    const uint8_t savedMaskQuest = randoCtx->GetOption(RSK_MASK_QUEST).Get();
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_DEFAULT);
    randoCtx->GetTrickOption(RT_BOMB_DETONATION).Set(0);
    randoCtx->GetTrickOption(RT_LENS_SHADOW_MQ).Set(1);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);

    // ---- VC1..VC5: RC_OGC_GS (castle_grounds.cpp), stricter with the trick off ----
    VcResetNoSwim();
    Give(RG_MASTER_SWORD);
    VcCheckOffOn(randoCtx, "VC1 OGC Gold Skulltula, adult with the Master Sword only, trick off",
                 "VC2 OGC Gold Skulltula, adult with the Master Sword only, trick on", RR_GANONS_CASTLE_GROUNDS,
                 RC_OGC_GS, true);
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(1);
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_OHKO);
    ExpectAt("VC3 OGC Gold Skulltula, adult with the Master Sword, trick on, OHKO and no fairy",
             RR_GANONS_CASTLE_GROUNDS, RC_OGC_GS, true, false,
             "the voidout is in logic under OHKO with no fairy (CanVoid)");
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_DEFAULT);
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);
    VcResetNoSwim();
    ExpectAt("VC4 OGC Gold Skulltula, adult with nothing, trick off", RR_GANONS_CASTLE_GROUNDS, RC_OGC_GS, true, false,
             "the Gold Skulltula is in logic with nothing to kill it");
    Give(RG_PROGRESSIVE_HOOKSHOT);
    ExpectAt("VC5 OGC Gold Skulltula, adult with the Hookshot, trick off", RR_GANONS_CASTLE_GROUNDS, RC_OGC_GS, true,
             true, "the Hookshot no longer kills and collects the Gold Skulltula without voiding (SoH e86e0ff69)");

    // ---- VC6..VC9: Hyrule Castle moat wonder items (castle_grounds.cpp) -------
    VcResetNoSwim();
    VcCheckOffOn(randoCtx, "VC6 HC moat wonder item 1, child without Swim, trick off",
                 "VC7 HC moat wonder item 1, child without Swim, trick on", RR_HC_MOAT, RC_HC_WONDER_MOAT_1, false);
    GiveSlingshot();
    VcCheckOffOn(randoCtx, "VC8 HC left torch wonder item, child with the slingshot, no Swim, trick off",
                 "VC9 HC left torch wonder item, child with the slingshot, no Swim, trick on", RR_HC_MOAT,
                 RC_HC_WONDER_LEFT_TORCH, false);

    // ---- VC10..VC17: Zora's River (zoras_river.cpp) ----------------------------
    VcResetNoSwim();
    VcCheckOffOn(randoCtx, "VC10 ZR wonder item near the domain 1, child without Swim, trick off",
                 "VC11 ZR wonder item near the domain 1, child without Swim, trick on", RR_ZORAS_RIVER,
                 RC_ZR_WONDER_NEAR_DOMAIN_1, false);
    VcCheckOffOn(randoCtx, "VC12 ZR lower river wonder item 1, child without Swim, trick off",
                 "VC13 ZR lower river wonder item 1, child without Swim, trick on", RR_ZR_FRONT,
                 RC_ZR_WONDER_LOWER_RIVER_1, false);
    VcCheckOffOn(randoCtx, "VC14 ZR beneath the waterfall left rupee, adult without Swim, trick off",
                 "VC15 ZR beneath the waterfall left rupee, adult without Swim, trick on", RR_ZORAS_RIVER,
                 RC_ZR_BENEATH_WATERFALL_LEFT_RUPEE, true);
    Give(RG_PROGRESSIVE_BOMB_BAG);
    randoCtx->GetTrickOption(RT_BOMB_DETONATION).Set(1);
    VcCheckOffOn(randoCtx, "VC16 ZR underwater rock 1, child with bombs and the detonation trick, no Swim, trick off",
                 "VC17 ZR underwater rock 1, child with bombs and the detonation trick, no Swim, trick on",
                 RR_ZORAS_RIVER, RC_ZR_UNDERWATER_ROCK_1, false);

    // ---- VC18..VC29: Gerudo Valley (gerudo_valley.cpp) -------------------------
    GiveGoronsBracelet();
    VcCheckOffOn(randoCtx, "VC18 GV underwater rock 1, child with bombs, bracelet and the detonation trick, trick off",
                 "VC19 GV underwater rock 1, child with bombs, bracelet and the detonation trick, trick on",
                 RR_GERUDO_VALLEY, RC_GV_UNDERWATER_ROCK_1, false);
    randoCtx->GetTrickOption(RT_BOMB_DETONATION).Set(0);
    VcResetNoSwim();
    VcCheckOffOn(randoCtx, "VC20 GV lower waterfall wonder item, adult without Swim, trick off",
                 "VC21 GV lower waterfall wonder item, adult without Swim, trick on", RR_GV_UPPER_STREAM,
                 RC_GV_WONDER_LOWER_WATERFALL, true);
    VcCheckOffOn(randoCtx, "VC22 GV upper waterfall wonder item in the alcove, adult without Swim, trick off",
                 "VC23 GV upper waterfall wonder item in the alcove, adult without Swim, trick on",
                 RR_GV_WATERFALL_ALCOVE, RC_GV_WONDER_UPPER_WATERFALL, true);
    VcCheckOffOn(randoCtx, "VC24 GV Octorok grotto front left blue rupee, adult without Swim, trick off",
                 "VC25 GV Octorok grotto front left blue rupee, adult without Swim, trick on", RR_GV_OCTOROK_GROTTO,
                 RC_GV_OCTOROK_GROTTO_FRONT_LEFT_BLUE_RUPEE, true);
    Give(RG_HOVER_BOOTS);
    VcCheckOffOn(randoCtx, "VC26 GV Octorok grotto red rupee, adult with Hover Boots, no Swim, trick off",
                 "VC27 GV Octorok grotto red rupee, adult with Hover Boots, no Swim, trick on", RR_GV_OCTOROK_GROTTO,
                 RC_GV_OCTOROK_GROTTO_RED_RUPEE, true);
    VcResetNoSwim();
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(1);
    ExpectAt("VC28 GV Octorok grotto red rupee, adult without Hover Boots or Swim, trick on", RR_GV_OCTOROK_GROTTO,
             RC_GV_OCTOROK_GROTTO_RED_RUPEE, true, false, "the far rupee is collected by voiding without Hover Boots");
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);
    VcResetNoSwim();
    logic->SetRandoInf(RAND_INF_CAN_SWIM, true);
    ExpectAt("VC29 GV upper waterfall wonder item in the alcove, adult with Swim, trick off", RR_GV_WATERFALL_ALCOVE,
             RC_GV_WONDER_UPPER_WATERFALL, true, true, "Swim no longer reaches the waterfall wonder item");

    // ---- VC30, VC31: the Colossus oasis (desert_colossus.cpp) ------------------
    VcResetNoSwim();
    GiveFairyOcarina();
    Give(RG_OCARINA_A_BUTTON);
    Give(RG_OCARINA_C_UP_BUTTON);
    Give(RG_OCARINA_C_DOWN_BUTTON);
    Give(RG_SONG_OF_STORMS);
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);
    ExpectExit("VC30 Colossus to the oasis, adult with Song of Storms, no Swim or bottle, trick off",
               RR_DESERT_COLOSSUS, RR_DESERT_COLOSSUS_OASIS, true, false,
               "the oasis fairies are in logic without Swim, Iron Boots, a bottle or the trick");
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(1);
    ExpectExit("VC31 Colossus to the oasis, adult with Song of Storms, no Swim or bottle, trick on", RR_DESERT_COLOSSUS,
               RR_DESERT_COLOSSUS_OASIS, true, true,
               "Voidout Collection does not reach the oasis fairies (SoH e86e0ff69)");
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);

    // ---- VC32..VC41: the Deku Tree before-boss hearts (deku_tree.cpp) ----------
    VcResetNoSwim();
    ExpectAt("VC32 Deku before-boss left heart, adult with nothing, trick off", RR_DEKU_TREE_OUTSIDE_BOSS_ROOM,
             RC_DEKU_TREE_BEFORE_BOSS_LEFT_HEART, true, true,
             "the adult no longer reaches the left heart without swimming (SoH e86e0ff69)");
    ExpectAt("VC33 Deku MQ before-boss right heart, adult with nothing, trick off", RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM,
             RC_DEKU_TREE_MQ_BEFORE_BOSS_RIGHT_HEART, true, true,
             "the adult no longer reaches the MQ right heart without swimming (SoH e86e0ff69)");
    VcCheckOffOn(randoCtx, "VC34 Deku before-boss right heart, child without Swim, trick off",
                 "VC35 Deku before-boss right heart, child without Swim, trick on", RR_DEKU_TREE_OUTSIDE_BOSS_ROOM,
                 RC_DEKU_TREE_BEFORE_BOSS_RIGHT_HEART, false);
    VcCheckOffOn(randoCtx, "VC36 Deku before-boss middle heart from basement upper, child, trick off",
                 "VC37 Deku before-boss middle heart from basement upper, child, trick on", RR_DEKU_TREE_BASEMENT_UPPER,
                 RC_DEKU_TREE_BEFORE_BOSS_MIDDLE_HEART, false);
    VcCheckOffOn(randoCtx, "VC38 Deku MQ before-boss middle heart from the basement ledge, child, trick off",
                 "VC39 Deku MQ before-boss middle heart from the basement ledge, child, trick on",
                 RR_DEKU_TREE_MQ_BASEMENT_LEDGE, RC_DEKU_TREE_MQ_BEFORE_BOSS_MIDDLE_HEART, false);
    VcCheckOffOn(randoCtx, "VC40 Deku before-boss middle heart, adult without Swim, trick off",
                 "VC41 Deku before-boss middle heart, adult without Swim, trick on", RR_DEKU_TREE_OUTSIDE_BOSS_ROOM,
                 RC_DEKU_TREE_BEFORE_BOSS_MIDDLE_HEART, true);

    // ---- VC42, VC43: the Forest Trial silver rupees (ganons_castle.cpp) --------
    VcResetNoSwim();
    FireEvents(RR_GANONS_CASTLE_FOREST_TRIAL_BEAMOS_ROOM, false);
    ExpectEvent("VC42 Forest Trial silver rupees event, child with nothing, trick off",
                LOGIC_GANONS_CASTLE_SILVER_FOREST, false,
                "the child collects the Forest Trial silver rupees with no Hookshot and no voidout");
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(1);
    FireEvents(RR_GANONS_CASTLE_FOREST_TRIAL_BEAMOS_ROOM, false);
    ExpectEvent("VC43 Forest Trial silver rupees event, child with nothing, trick on",
                LOGIC_GANONS_CASTLE_SILVER_FOREST, true,
                "the child's voidout after the switch does not collect the silver rupees (SoH e86e0ff69)");
    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(0);

    // ---- VC44..VC48: the Shadow MQ Gold Skulltulas (shadow_temple.cpp) ---------
    VcResetNoSwim();
    Give(RG_PROGRESSIVE_BOMB_BAG);
    VcCheckOffOn(randoCtx, "VC44 Shadow MQ Gold Skulltula after the ship, adult with bombs, trick off",
                 "VC45 Shadow MQ Gold Skulltula after the ship, adult with bombs, trick on",
                 RR_SHADOW_TEMPLE_MQ_BEYOND_BOAT, RC_SHADOW_TEMPLE_MQ_GS_AFTER_SHIP, true);
    VcCheckOffOn(randoCtx, "VC46 Shadow MQ Gold Skulltula near the boss, adult with bombs, trick off",
                 "VC47 Shadow MQ Gold Skulltula near the boss, adult with bombs, trick on",
                 RR_SHADOW_TEMPLE_MQ_BOSS_DOOR, RC_SHADOW_TEMPLE_MQ_GS_NEAR_BOSS, true);
    Give(RG_PROGRESSIVE_HOOKSHOT);
    ExpectAt("VC48 Shadow MQ Gold Skulltula near the boss, adult with bombs and the Hookshot, trick off",
             RR_SHADOW_TEMPLE_MQ_BOSS_DOOR, RC_SHADOW_TEMPLE_MQ_GS_NEAR_BOSS, true, true,
             "the Hookshot no longer collects the Gold Skulltula without voiding (SoH e86e0ff69)");

    // ---- VC49..VC54: the Thieves' Hideout kitchen (thieves_hideout.cpp, SoH eca767dcb) ----
    VcResetNoSwim();
    Give(RG_PROGRESSIVE_BOMB_BAG);
    VcCheckOffOn(randoCtx, "VC49 TH kitchen pot 1 from by the corridor, adult with bombs, trick off",
                 "VC50 TH kitchen pot 1 from by the corridor, adult with bombs, trick on", RR_TH_KITCHEN_BY_CORRIDOR,
                 RC_TH_KITCHEN_POT_1, true);
    VcCheckOffOn(randoCtx, "VC51 TH kitchen pot 2 from across the corridor, adult with bombs, trick off",
                 "VC52 TH kitchen pot 2 from across the corridor, adult with bombs, trick on",
                 RR_TH_KITCHEN_OPPOSITE_CORRIDOR, RC_TH_KITCHEN_POT_2, true);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(RO_MASK_QUEST_SHUFFLE);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    VcResetNoSwim();
    logic->SetRandoInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY, true);
    Give(RG_HOVER_BOOTS);
    VcCheckOffOn(randoCtx, "VC53 TH kitchen soup, adult with Hover Boots and the hood, both hood settings, trick off",
                 "VC54 TH kitchen soup, adult with Hover Boots and the hood, both hood settings, trick on",
                 RR_TH_KITCHEN_BY_CORRIDOR, RC_TH_WONDER_KITCHEN_SOUP, true);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(savedMaskQuest);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);

    // ---- VC55..VC58: the MQ Fire Trial pillar rupee (ganons_castle.cpp, the Fire Trial split's regions) ----
    const uint8_t savedHoverBoost = randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Get();
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(1);
    VcResetNoSwim();
    Give(RG_GORON_TUNIC);
    VcCheckOffOn(randoCtx,
                 "VC55 MQ Fire Trial on-pillar rupee from the open door, tunic, no Golden Gauntlets, trick off",
                 "VC56 MQ Fire Trial on-pillar rupee from the open door, tunic, no Golden Gauntlets, trick on",
                 RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RC_GANONS_CASTLE_MQ_ON_PILLAR_FIRE_SILVER, true);
    Give(RG_MEGATON_HAMMER);
    Give(RG_HOVER_BOOTS);
    VcCheckOffOn(randoCtx,
                 "VC57 MQ Fire Trial on-pillar rupee from the barred door, tunic, hammer recoil hover, no Golden "
                 "Gauntlets, trick off",
                 "VC58 MQ Fire Trial on-pillar rupee from the barred door, tunic, hammer recoil hover, no Golden "
                 "Gauntlets, trick on",
                 RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED, RC_GANONS_CASTLE_MQ_ON_PILLAR_FIRE_SILVER, true);
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(savedHoverBoost);

    randoCtx->GetTrickOption(RT_VOIDOUT_COLLECTION).Set(savedVoidout);
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(savedDamage);
    randoCtx->GetTrickOption(RT_BOMB_DETONATION).Set(savedDetonation);
    randoCtx->GetTrickOption(RT_LENS_SHADOW_MQ).Set(savedLensShadowMq);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(savedBunnyHood);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(savedMasksAsAdult);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(savedMaskQuest);
}

// ---- SoH 156c2a4a (#7417) and 28ab1206 (#7396): LH, GV and GC -------------
void RunSohLogicBatchLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedHoverBoost = randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Get();
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);

    // ---- LH: the grass Epona tramples (lake_hylia.cpp, SummonEpona) ---------
    ResetLogic();
    GiveFairyOcarina();
    Give(RG_OCARINA_C_LEFT_BUTTON);
    Give(RG_OCARINA_C_RIGHT_BUTTON);
    Give(RG_OCARINA_C_UP_BUTTON);
    Give(RG_EPONAS_SONG);
    ExpectAs(true, "LH1 Lake Hylia grass 1, adult with ocarina and Epona's Song, Epona not freed", RR_LAKE_HYLIA,
             RC_LH_GRASS_1, false, "the grass is in logic for an adult with nothing that cuts it and no Epona");
    logic->Set(LOGIC_FREED_EPONA, true);
    ExpectAs(true, "LH2 Lake Hylia grass 1, adult who can summon Epona", RR_LAKE_HYLIA, RC_LH_GRASS_1, true,
             "an adult riding Epona cannot cut the lake grass (SoH 156c2a4a)");

    // ---- GV: the rocks across the bridge need CanBreakRocks (gerudo_valley.cpp)
    ResetLogic();
    ExpectAs(true, "GV1 GV rock across the bridge 1, adult with nothing", RR_GV_FORTRESS_SIDE,
             RC_GV_ROCK_ACROSS_BRIDGE_1, false, "the rock is in logic for an adult who cannot break or lift it");
    Give(RG_PROGRESSIVE_BOMB_BAG);
    ExpectAs(true, "GV2 GV rock across the bridge 1, adult with a bomb bag", RR_GV_FORTRESS_SIDE,
             RC_GV_ROCK_ACROSS_BRIDGE_1, true, "an adult with bombs cannot break the rock (SoH 156c2a4a)");

    // ---- GC: the MQ Fire Trial's From Barred Door side (ganons_castle.cpp) ---
    ResetLogic();
    ExpectExit("GC1 MQ Fire Trial Final Room to From Barred Door, adult", RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FINAL_ROOM,
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED, true, true,
               "the Final Room no longer leads back to the barred-door side of the main room (SoH 28ab1206)");
    Give(RG_GORON_TUNIC);
    Give(RG_MEGATON_HAMMER);
    Give(RG_HOVER_BOOTS);
    ExpectExit("GC2 MQ Fire Trial From Barred Door to Open Door, tunic, hammer, Hover Boots, no hover boost",
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_OPEN_DOOR, true, false,
               "the lava is crossed back from the barred door with no hammer recoil hover");
    ExpectAs(true, "GC3 MQ Fire Trial launch platform rupee from the barred door, no hover boost",
             RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED, RC_GANONS_CASTLE_MQ_LAUNCH_PLATFORM_FIRE_SILVER, false,
             "the rupee is in logic from the barred door with no hammer recoil hover");
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(1);
    ExpectExit("GC4 MQ Fire Trial From Barred Door to Open Door, with RT_HOVER_BOOST_SIMPLE",
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_OPEN_DOOR, true, true,
               "a hammer recoil hover does not carry the adult back across the lava");
    ExpectAs(true, "GC5 MQ Fire Trial launch platform rupee from the barred door, with RT_HOVER_BOOST_SIMPLE",
             RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_BARRED, RC_GANONS_CASTLE_MQ_LAUNCH_PLATFORM_FIRE_SILVER, true,
             "a hammer recoil hover does not reach the launch platform rupee from the barred door");

    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(savedHoverBoost);
}

// ---- SoH 139381a0e (#6336) and 88099e6eb (#6487): the Fire Trial split, FT ---
void FtsGiveLongshot() {
    Give(RG_PROGRESSIVE_HOOKSHOT);
    Give(RG_PROGRESSIVE_HOOKSHOT);
}

/** Golden Gauntlets under Shuffle Grab: the first strength grants grab. */
bool FtsGiveGoldenGauntlets() {
    for (int i = 0; i < 4 && !logic->HasItem(RG_GOLDEN_GAUNTLETS); i++) {
        Give(RG_PROGRESSIVE_STRENGTH);
    }
    return logic->HasItem(RG_GOLDEN_GAUNTLETS);
}

/** A reset with @p startingHearts (the option's value: 2 is three hearts). */
void FtsReset(const std::shared_ptr<Rando::Context>& randoCtx, uint8_t startingHearts) {
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(startingHearts);
    ResetLogic();
}

/** The whole trial for an adult: Goron Tunic, Longshot, Golden Gauntlets and Light Arrows. */
void FtsTrialWalk(const std::shared_ptr<Rando::Context>& randoCtx, const char* legReach, const char* legClear,
                  RandomizerRegion mainRoom, RandomizerRegion finalRoom) {
    FtsReset(randoCtx, 2);
    Give(RG_GORON_TUNIC);
    FtsGiveLongshot();
    Give(RG_PROGRESSIVE_BOW);
    Give(RG_PROGRESSIVE_MAGIC_METER);
    Give(RG_LIGHT_ARROWS);
    if (!FtsGiveGoldenGauntlets() || !logic->HasItem(RG_LIGHT_ARROWS) || !logic->HasItem(RG_FAIRY_BOW) ||
        !logic->HasItem(RG_MAGIC_SINGLE)) {
        GateFail("%s: the gauntlets, bow, magic or Light Arrows did not take, so the walk is vacuous", legReach);
        return;
    }
    ExpectSearch(legReach, mainRoom, true, finalRoom, RC_UNKNOWN_CHECK, true,
                 "an adult with everything the trial needs cannot reach its Final Room from the main room");
    ExpectEvent(legClear, LOGIC_FIRE_TRIAL_CLEAR, true,
                "the search reached the Final Room but the trial is not cleared with Light Arrows");
}

void RunFireTrialSplitLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedHearts = randoCtx->GetOption(RSK_STARTING_HEARTS).Get();
    const uint8_t savedDamage = randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Get();
    const uint8_t savedSilver = randoCtx->GetOption(RSK_SHUFFLE_SILVER).Get();
    const uint8_t savedLocked = randoCtx->GetOption(RSK_MEDALLION_LOCKED_TRIALS).Get();
    const uint8_t savedBunny = randoCtx->GetOption(RSK_BUNNY_HOOD).Get();
    const uint8_t savedMaskQuest = randoCtx->GetOption(RSK_MASK_QUEST).Get();
    const uint8_t savedMasksAsAdult = randoCtx->GetOption(RSK_MASKS_AS_ADULT).Get();
    const uint8_t savedTunic = randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Get();
    const uint8_t savedHoverBoost = randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Get();
    const uint8_t savedMqFire = randoCtx->GetTrickOption(RT_GANON_MQ_FIRE_TRIAL).Get();
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_DEFAULT);
    randoCtx->GetOption(RSK_SHUFFLE_SILVER).Set(RO_SHUFFLE_SILVER_OFF);
    randoCtx->GetOption(RSK_MEDALLION_LOCKED_TRIALS).Set(0);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(0);
    // The hood is held by Mask Quest Shuffle's flag and worn by the adult under Masks as Adult (FT19, FT20).
    randoCtx->GetOption(RSK_MASK_QUEST).Set(RO_MASK_QUEST_SHUFFLE);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(0);
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);
    randoCtx->GetTrickOption(RT_GANON_MQ_FIRE_TRIAL).Set(0);

    // ---- vanilla: the heart piece and the longshot to the barred door ------
    FtsReset(randoCtx, 2); // three hearts: FireTimer 24 under the trick
    ExpectAs(true, "FT1 Fire Trial heart from the open door, adult, three hearts, no tunic, no trick",
             RR_GANONS_CASTLE_FIRE_TRIAL_FROM_OPEN, RC_GANONS_CASTLE_FIRE_TRIAL_HEART, false,
             "the heart piece over the lava is in logic with no Goron Tunic and no RT_FEWER_TUNIC_REQUIREMENTS");
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(1);
    ExpectAs(true,
             "FT2 Fire Trial heart from the open door, adult, three hearts, no tunic, RT_FEWER_TUNIC_REQUIREMENTS",
             RR_GANONS_CASTLE_FIRE_TRIAL_FROM_OPEN, RC_GANONS_CASTLE_FIRE_TRIAL_HEART, true,
             "three hearts (FireTimer 24) under the trick do not reach the heart piece (SoH 139381a0e)");
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(0);
    FtsGiveLongshot();
    ExpectExit("FT3 Fire Trial From Open Door to Barred Door, adult, Longshot, no tunic, no trick",
               RR_GANONS_CASTLE_FIRE_TRIAL_FROM_OPEN, RR_GANONS_CASTLE_FIRE_TRIAL_BARRED_DOOR, true, false,
               "the longshot across the lava is in logic with no Goron Tunic and no RT_FEWER_TUNIC_REQUIREMENTS");
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(1);
    ExpectExit(
        "FT4 Fire Trial From Open Door to Barred Door, adult, Longshot, three hearts, RT_FEWER_TUNIC_REQUIREMENTS",
        RR_GANONS_CASTLE_FIRE_TRIAL_FROM_OPEN, RR_GANONS_CASTLE_FIRE_TRIAL_BARRED_DOOR, true, true,
        "the Longshot and FireTimer 24 do not reach the barred door (SoH 139381a0e)");
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(0);

    // ---- vanilla: eca767dcb's hammer recoil hover back to the open door ----
    FtsReset(randoCtx, 2);
    Give(RG_GORON_TUNIC);
    Give(RG_MEGATON_HAMMER);
    Give(RG_HOVER_BOOTS);
    ExpectExit("FT5 Fire Trial From Barred Door to Open Door, tunic, hammer, Hover Boots, no hover boost",
               RR_GANONS_CASTLE_FIRE_TRIAL_FROM_BARRED, RR_GANONS_CASTLE_FIRE_TRIAL_OPEN_DOOR, true, false,
               "the lava is crossed back from the barred door with no Longshot and no hammer recoil hover");
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(1);
    ExpectExit("FT6 Fire Trial From Barred Door to Open Door, tunic, hammer, Hover Boots, RT_HOVER_BOOST_SIMPLE",
               RR_GANONS_CASTLE_FIRE_TRIAL_FROM_BARRED, RR_GANONS_CASTLE_FIRE_TRIAL_OPEN_DOOR, true, true,
               "a hammer recoil hover does not carry the adult back across the lava (SoH eca767dcb)");
    ExpectAs(true, "FT17 Fire Trial heart from the barred door, tunic, hammer, Hover Boots, RT_HOVER_BOOST_SIMPLE",
             RR_GANONS_CASTLE_FIRE_TRIAL_FROM_BARRED, RC_GANONS_CASTLE_FIRE_TRIAL_HEART, true,
             "a hammer recoil hover does not reach the heart piece from the barred door (SoH eca767dcb)");
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);
    ExpectAs(true, "FT18 Fire Trial heart from the barred door, tunic, hammer, Hover Boots, no hover boost",
             RR_GANONS_CASTLE_FIRE_TRIAL_FROM_BARRED, RC_GANONS_CASTLE_FIRE_TRIAL_HEART, false,
             "the heart piece is in logic from the barred door with no Longshot and no hammer recoil hover");

    // ---- vanilla: eca767dcb's Bunny Hood hover to the distant platform -------
    FtsReset(randoCtx, 2);
    Give(RG_GORON_TUNIC);
    Give(RG_HOVER_BOOTS);
    logic->SetRandoInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY, true);
    ExpectAs(true, "FT19 Fire Trial distant platform rupee, tunic, Hover Boots, hood, Bunny Hood Effect off",
             RR_GANONS_CASTLE_FIRE_TRIAL_FROM_OPEN, RC_GANONS_CASTLE_DISTANT_PLATFORM_FIRE_SILVER, false,
             "the distant platform rupee is in logic with no Golden Gauntlets and no Bunny Hood hover");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectAs(true, "FT20 Fire Trial distant platform rupee, tunic, Hover Boots, hood, Bunny Hood Effect on",
             RR_GANONS_CASTLE_FIRE_TRIAL_FROM_OPEN, RC_GANONS_CASTLE_DISTANT_PLATFORM_FIRE_SILVER, true,
             "a Bunny Hood hover does not reach the distant platform rupee (SoH eca767dcb)");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);

    // ---- vanilla: the whole trial from the main room -----------------------
    FtsTrialWalk(randoCtx, "FT7 Fire Trial walk, main room to Final Room", "FT8 Fire Trial cleared",
                 RR_GANONS_CASTLE_MAIN, RR_GANONS_CASTLE_FIRE_TRIAL_FINAL_ROOM);

    // ---- Master Quest: From Open Door to the barred door ---------------------
    FtsReset(randoCtx, 2);
    ExpectExit("FT9 MQ main room to Fire Trial From Open Door, adult, trials not medallion-locked",
               RR_GANONS_CASTLE_MQ_MAIN, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, true, true,
               "the main room's fire trial door does not lead into the trial room (SoH 27d35e5a9)");
    Give(RG_GORON_TUNIC);
    ExpectExit("FT10 MQ Fire Trial From Open Door to Barred Door, adult, tunic only",
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_BARRED_DOOR, true, false,
               "the barred door is in logic with no Longshot, Hover Boots or Golden Gauntlets");
    FtsGiveLongshot();
    ExpectExit("FT11 MQ Fire Trial From Open Door to Barred Door, adult, tunic and Longshot",
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_BARRED_DOOR, true, true,
               "the Goron Tunic and the Longshot do not reach the barred door");
    FtsReset(randoCtx, 2);
    FtsGiveLongshot();
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(1);
    ExpectExit("FT12 MQ Fire Trial From Open Door to Barred Door, Longshot, three hearts, RT_FEWER_TUNIC_REQUIREMENTS",
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_BARRED_DOOR, true, false,
               "FireTimer 24 (three hearts) is enough for the barred door, which needs 32");
    FtsReset(randoCtx, 3); // four hearts: FireTimer 32 under the trick
    FtsGiveLongshot();
    ExpectExit("FT13 MQ Fire Trial From Open Door to Barred Door, Longshot, four hearts, RT_FEWER_TUNIC_REQUIREMENTS",
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_BARRED_DOOR, true, true,
               "FireTimer 32 with no Goron Tunic does not reach the barred door: the tunic test is still there "
               "(SoH 139381a0e)");
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(0);
    FtsReset(randoCtx, 2);
    Give(RG_GORON_TUNIC);
    Give(RG_HOVER_BOOTS);
    ExpectExit("FT14 MQ Fire Trial From Open Door to Barred Door, tunic, Hover Boots, three hearts",
               RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_BARRED_DOOR, true, true,
               "Hover Boots and a hit to take (TakeDamage) do not reach the barred door (SoH develop)");

    // ---- Master Quest: the under-pillar rupee needs the Golden Gauntlets -----
    FtsReset(randoCtx, 2);
    Give(RG_GORON_TUNIC);
    ExpectAs(true, "FT21 MQ Fire Trial under-pillar rupee from the open door, tunic, no Golden Gauntlets",
             RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RC_GANONS_CASTLE_MQ_UNDER_PILLAR_FIRE_SILVER, false,
             "the rupee under the pillar is in logic with no way to lift the pillar");
    if (!FtsGiveGoldenGauntlets()) {
        GateFail("FT22: the Golden Gauntlets did not take, so the leg is vacuous");
    } else {
        ExpectAs(true, "FT22 MQ Fire Trial under-pillar rupee from the open door, tunic, Golden Gauntlets",
                 RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FROM_OPEN, RC_GANONS_CASTLE_MQ_UNDER_PILLAR_FIRE_SILVER, true,
                 "the Goron Tunic and the Golden Gauntlets do not reach the rupee under the pillar (SoH develop)");
    }

    // ---- Master Quest: the whole trial from the main room (88099e6eb) --------
    FtsTrialWalk(randoCtx, "FT15 MQ Fire Trial walk, main room to Final Room", "FT16 MQ Fire Trial cleared",
                 RR_GANONS_CASTLE_MQ_MAIN, RR_GANONS_CASTLE_MQ_FIRE_TRIAL_FINAL_ROOM);

    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(savedHearts);
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(savedDamage);
    randoCtx->GetOption(RSK_SHUFFLE_SILVER).Set(savedSilver);
    randoCtx->GetOption(RSK_MEDALLION_LOCKED_TRIALS).Set(savedLocked);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(savedBunny);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(savedMaskQuest);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(savedMasksAsAdult);
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(savedTunic);
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(savedHoverBoost);
    randoCtx->GetTrickOption(RT_GANON_MQ_FIRE_TRIAL).Set(savedMqFire);
}

// ---- SoH eca767dcb (Bunny Hood Effect) and e7f1801f0 (#7488): BH and LW -----
void ExpectMode(const char* leg, int got, int expected, const char* why) {
    if (got != expected) {
        GateFail("%s: %s (expected mode %d, got %d)", leg, why, expected, got);
        return;
    }
    printf("[TEST]   ok: %s: mode %d\n", leg, got);
}

void ExpectFlag(const char* leg, bool got, bool expected, const char* why) {
    if (got != expected) {
        GateFail("%s: %s (expected %s, got %s)", leg, why, expected ? "true" : "false", got ? "true" : "false");
        return;
    }
    printf("[TEST]   ok: %s: %s\n", leg, got ? "true" : "false");
}

/** ResetLogic, holding the Bunny Hood (Mask Quest Shuffle's ownership flag). */
void ResetLogicWithBunnyHood() {
    ResetLogic();
    logic->SetRandoInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY, true);
}

// ---- #1647 (SoH eca767dcb stages 1 and 2, 73a739b7a): BL, WE, HW, OW, RH, D1 ----
// The tricks these legs force (saved and restored), so a trick the context holds
// on cannot open an exit a leg expects closed.
const RandomizerTrick kBhlForcedTricks[] = {
    RT_DEKU_B1_SKIP,
    RT_GROUND_JUMP,
    RT_GROUND_JUMP_HARD,
    RT_UNINTUITIVE_JUMPS,
    RT_FOREST_OUTSIDE_BACKDOOR,
    RT_HW_CROSSING,
    RT_HW_BUNNY_CROSSING,
    RT_WATER_HIGH_EMBLEM_JUMP,
    RT_HOVER_BOOST_SIMPLE,
    RT_DAMAGE_BOOST_SIMPLE,
    RT_GY_CHILD_DAMPE_RACE_POH,
    RT_DMT_BOMBABLE,
    RT_SPIRIT_MQ_SUN_BLOCK_SOT,
    RT_FOREST_DOORFRAME,
};

/** One exit for a child holding the hood: closed with Bunny Hood Effect off, open with it on. */
void BhlExitOffOn(const std::shared_ptr<Rando::Context>& randoCtx, const char* legOff, const char* legOn,
                  RandomizerRegion from, RandomizerRegion to) {
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectExit(legOff, from, to, false, false, "the hood's jump is in logic in a seed without Bunny Hood Effect");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit(legOn, from, to, false, true, "the child's hood jump is not in logic under Bunny Hood Effect (#1647)");
}

/** One location for a child holding the hood: out of logic with the setting off, in logic with it on. */
void BhlCheckOffOn(const std::shared_ptr<Rando::Context>& randoCtx, const char* legOff, const char* legOn,
                   RandomizerRegion region, RandomizerCheck rc) {
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectAt(legOff, region, rc, false, false, "the hood's jump is in logic in a seed without Bunny Hood Effect");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectAt(legOn, region, rc, false, true, "the child's hood jump is not in logic under Bunny Hood Effect (#1647)");
}

/** A helper of Logic asked directly, as one age, with a region current (BAllowed reads its scene). */
void BhlAs(bool adult, RandomizerRegion region) {
    logic->IsChild = !adult;
    logic->IsAdult = adult;
    logic->CurrentRegionKey = region;
}

/** BhlExitOffOn for an exit whose lambda asks AnyAgeTime: the region holds the child's access for the probe. */
void BhlExitOffOnAsChild(const std::shared_ptr<Rando::Context>& randoCtx, const char* legOff, const char* legOn,
                         RandomizerRegion from, RandomizerRegion to) {
    Region* region = RegionTable(from);
    const bool saved[4] = { region->childDay, region->childNight, region->adultDay, region->adultNight };
    region->childDay = true;
    region->childNight = true;
    region->adultDay = false;
    region->adultNight = false;
    BhlExitOffOn(randoCtx, legOff, legOn, from, to);
    region->childDay = saved[0];
    region->childNight = saved[1];
    region->adultDay = saved[2];
    region->adultNight = saved[3];
}

void RunBunnyHoodLogicLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    uint8_t savedTricks[sizeof(kBhlForcedTricks) / sizeof(kBhlForcedTricks[0])];
    for (size_t i = 0; i < sizeof(kBhlForcedTricks) / sizeof(kBhlForcedTricks[0]); i++) {
        savedTricks[i] = randoCtx->GetTrickOption(kBhlForcedTricks[i]).Get();
        randoCtx->GetTrickOption(kBhlForcedTricks[i]).Set(0);
    }

    // ---- BL: the live child Bunny Hood terms, one pair per area ----------------
    ResetLogicWithBunnyHood();
    BhlExitOffOn(randoCtx, "BL1 Deku Tree basement lower to upper, child with the hood, setting off",
                 "BL2 Deku Tree basement lower to upper, child with the hood, setting on", RR_DEKU_TREE_BASEMENT_LOWER,
                 RR_DEKU_TREE_BASEMENT_UPPER);
    BhlCheckOffOn(randoCtx, "BL3 DC MQ upper Lizalfos heart, child with the hood, setting off",
                  "BL4 DC MQ upper Lizalfos heart, child with the hood, setting on",
                  RR_DODONGOS_CAVERN_MQ_UPPER_LIZALFOS, RC_DODONGOS_CAVERN_MQ_LIZALFOS_ROOM_HEART);
    BhlExitOffOn(randoCtx, "BL5 Fire SoT cage upper door to switch, child with the hood, no ocarina, setting off",
                 "BL6 Fire SoT cage upper door to switch, child with the hood, no ocarina, setting on",
                 RR_FIRE_TEMPLE_SOT_CAGE_UPPER_DOOR, RR_FIRE_TEMPLE_SOT_CAGE_SWITCH);
    BhlExitOffOn(randoCtx, "BL9 GTG MQ ledge-side to furthest platform, child with the hood, setting off",
                 "BL10 GTG MQ ledge-side to furthest platform, child with the hood, setting on",
                 RR_GERUDO_TRAINING_GROUND_MQ_LEDGE_SIDE_PLATFORMS, RR_GERUDO_TRAINING_GROUND_MQ_FURTHEST_PLATFORM);
    BhlCheckOffOn(randoCtx, "BL11 Ice Cavern near-block silver rupee, child with the hood, no grab, setting off",
                  "BL12 Ice Cavern near-block silver rupee, child with the hood, no grab, setting on",
                  RR_ICE_CAVERN_BLOCK_ROOM, RC_ICE_CAVERN_NEAR_BLOCK_SILVER);
    BhlCheckOffOn(randoCtx, "BL13 Shadow MQ east airborne spikes rupee, child with the hood only, setting off",
                  "BL14 Shadow MQ east airborne spikes rupee, child with the hood only, setting on",
                  RR_SHADOW_TEMPLE_MQ_SPIKES_DOOR_PLATFORM, RC_SHADOW_MQ_E_AIRBORNE_SPIKES_SILVER);
    BhlExitOffOn(randoCtx, "BL15 Spirit GS ledge to child statue room, child with the hood, setting off",
                 "BL16 Spirit GS ledge to child statue room, child with the hood, setting on",
                 RR_SPIRIT_TEMPLE_GS_LEDGE, RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD);
    logic->SetRandoInf(RAND_INF_CAN_SWIM, false);
    BhlExitOffOn(randoCtx, "BL17 Water Like Like spikes to toggle switch, child with the hood, no swim, setting off",
                 "BL18 Water Like Like spikes to toggle switch, child with the hood, no swim, setting on",
                 RR_WATER_TEMPLE_LIKE_LIKE_SPIKES, RR_WATER_TEMPLE_TOGGLE_SWITCH);
    // The two terms that also need a jumpslash: closed with the hood alone, open with the sword.
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit("BL19 Forest block-push room to courtyard alcove, child with the hood but no jumpslash, setting on",
               RR_FOREST_TEMPLE_MIDDLE_BLOCK_PUSH_ROOM, RR_FOREST_TEMPLE_BLOCK_PUSH_ROOM_COURTYARD_ALCOVE, false, false,
               "the hood alone crosses to the alcove: upstream asks for a jumpslash too");
    GiveKokiriSword();
    BhlExitOffOn(randoCtx, "BL20 Forest block-push room to courtyard alcove, child with hood and sword, setting off",
                 "BL21 Forest block-push room to courtyard alcove, child with hood and sword, setting on",
                 RR_FOREST_TEMPLE_MIDDLE_BLOCK_PUSH_ROOM, RR_FOREST_TEMPLE_BLOCK_PUSH_ROOM_COURTYARD_ALCOVE);
    BhlExitOffOn(randoCtx, "BL22 Ganon's Shadow Trial end to lower switch, child with hood and sword, setting off",
                 "BL23 Ganon's Shadow Trial end to lower switch, child with hood and sword, setting on",
                 RR_GANONS_CASTLE_SHADOW_TRIAL_END, RR_GANONS_CASTLE_SHADOW_TRIAL_LOWER_SWITCH);
    // Jabu MQ lift room: the hood's term rides RT_UNINTUITIVE_JUMPS.
    ResetLogicWithBunnyHood();
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit("BL24 Jabu MQ lift room to east ledge, child with the hood, setting on, no RT_UNINTUITIVE_JUMPS",
               RR_JABU_JABUS_BELLY_MQ_LIFT_ROOM, RR_JABU_JABUS_BELLY_MQ_LIFT_ROOM_EAST_LEDGE, false, false,
               "the lift-room jump is in logic without Unintuitive Jumps");
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(1);
    BhlExitOffOn(randoCtx, "BL25 Jabu MQ lift room to east ledge, child with hood, RT_UNINTUITIVE_JUMPS, setting off",
                 "BL26 Jabu MQ lift room to east ledge, child with hood, RT_UNINTUITIVE_JUMPS, setting on",
                 RR_JABU_JABUS_BELLY_MQ_LIFT_ROOM, RR_JABU_JABUS_BELLY_MQ_LIFT_ROOM_EAST_LEDGE);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    // MQ Spirit statue to the sun block (logic.cpp, upstream form): the bracelet and the hood.
    ResetLogicWithBunnyHood();
    GiveGrab();
    BhlAs(false, RR_SPIRIT_TEMPLE_MQ_STATUE_ROOM_CHILD);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectFlag("BL27 MQSpiritStatueToSunBlock, child with bracelet and hood, no ocarina, setting off",
               logic->MQSpiritStatueToSunBlock(), false, "the hood's jump to the sun block is in logic with it off");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectFlag("BL28 MQSpiritStatueToSunBlock, child with bracelet and hood, no ocarina, setting on",
               logic->MQSpiritStatueToSunBlock(), true, "the child's hood jump to the sun block is not in logic");
    logic->CurrentRegionKey = RR_NONE;
    // Deku MQ compass-room boulder: Boomerang and bombs, the hood in place of the Song of Time. Bombchu drops
    // are held off so the bomb bag cannot answer through bombchus.
    const uint8_t savedChuDrops = randoCtx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Get();
    randoCtx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Set(RO_AMMO_DROPS_OFF);
    ResetLogicWithBunnyHood();
    Give(RG_BOOMERANG);
    Give(RG_PROGRESSIVE_BOMB_BAG);
    BhlCheckOffOn(randoCtx, "BL29 Deku MQ compass room boulder, child with Boomerang, bombs and hood, setting off",
                  "BL30 Deku MQ compass room boulder, child with Boomerang, bombs and hood, setting on",
                  RR_DEKU_TREE_MQ_COMPASS_ROOM, RC_DEKU_TREE_MQ_BOULDER_1);
    randoCtx->GetOption(RSK_ENABLE_BOMBCHU_DROPS).Set(savedChuDrops);
    // DC MQ lobby: Goron's Bracelet and the hood past the lower-right mud wall (an AnyAgeTime exit).
    ResetLogicWithBunnyHood();
    GiveGoronsBracelet();
    BhlExitOffOnAsChild(randoCtx,
                        "BL31 DC MQ lobby to lower right side, child with Goron's Bracelet and hood, setting off",
                        "BL32 DC MQ lobby to lower right side, child with Goron's Bracelet and hood, setting on",
                        RR_DODONGOS_CAVERN_MQ_LOBBY, RR_DODONGOS_CAVERN_MQ_LOWER_RIGHT_SIDE);
    // DC MQ lobby: the bomb-flower ground jump the hood stands in for RT_GROUND_JUMP_HARD in.
    // The shield's equipment bit is set directly: Logic::ApplyItemEffect ignores a Deku or Hylian Shield that does not
    // come off a shop shelf (a shield can burn, so only a shop counts as a source), and Give() would do nothing.
    logic->GetSaveContext()->inventory.equipment |= EQUIP_FLAG_SHIELD_DEKU;
    randoCtx->GetTrickOption(RT_GROUND_JUMP).Set(1);
    BhlExitOffOn(randoCtx, "BL33 DC MQ lobby to outside Poes room, child with bracelet, shield, hood, ground jump, off",
                 "BL34 DC MQ lobby to outside Poes room, child with bracelet, shield, hood, ground jump, on",
                 RR_DODONGOS_CAVERN_MQ_LOBBY, RR_DODONGOS_CAVERN_MQ_OUTSIDE_POES_ROOM);
    randoCtx->GetTrickOption(RT_GROUND_JUMP).Set(0);
    // Water river pots to above the dragon: the slingshot and the hood.
    ResetLogicWithBunnyHood();
    GiveSlingshot();
    BhlExitOffOn(randoCtx, "BL35 Water river pots to above the dragon, child with slingshot and hood, setting off",
                 "BL36 Water river pots to above the dragon, child with slingshot and hood, setting on",
                 RR_WATER_TEMPLE_RIVER_POTS, RR_WATER_TEMPLE_ABOVE_DRAGON);

    // ---- OW: the overworld terms -----------------------------------------------
    ResetLogicWithBunnyHood();
    BhlExitOffOn(randoCtx, "OW1 Kakariko to the backyard, child with the hood, no bracelet, setting off",
                 "OW2 Kakariko to the backyard, child with the hood, no bracelet, setting on", RR_KAKARIKO_VILLAGE,
                 RR_KAK_BACKYARD);
    GiveNut();
    BhlCheckOffOn(randoCtx, "OW3 Dampe race Piece of Heart, child with the hood and nut, setting off",
                  "OW4 Dampe race Piece of Heart, child with the hood and nut, setting on", RR_GRAVEYARD_DAMPES_GRAVE,
                  RC_GRAVEYARD_DAMPE_RACE_FREESTANDING_POH);
    ResetLogicWithBunnyHood();
    // The first Progressive Strength is the grab (ResetLogic withholds it), the second Goron's Bracelet.
    GiveGoronsBracelet();
    logic->SetRandoInf(RAND_INF_CAN_OPEN_CHEST, true);
    BhlCheckOffOn(randoCtx, "OW5 DMT chest, child with Goron's Bracelet and the hood, no trick, setting off",
                  "OW6 DMT chest, child with Goron's Bracelet and the hood, no trick, setting on",
                  RR_DEATH_MOUNTAIN_TRAIL, RC_DMT_CHEST);
    ResetLogicWithBunnyHood();
    logic->SetRandoInf(RAND_INF_CAN_SWIM, false);
    logic->SetRandoInf(RAND_INF_CAN_CLIMB, false);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit("OW7 ZD island to Zora's Domain, child with the hood, no swim, no Climb, setting on",
               RR_ZORAS_DOMAIN_ISLAND, RR_ZORAS_DOMAIN, false, false,
               "the hood's jump back off the island needs no climb (upstream asks for Climb)");
    GiveClimb();
    BhlExitOffOn(randoCtx, "OW8 ZD island to Zora's Domain, child with hood and Climb, no swim, setting off",
                 "OW9 ZD island to Zora's Domain, child with hood and Climb, no swim, setting on",
                 RR_ZORAS_DOMAIN_ISLAND, RR_ZORAS_DOMAIN);
    ResetLogicWithBunnyHood();
    GiveKokiriSword();
    BhlExitOffOn(randoCtx, "OW10 ZR atop the ladder to the pillar, child with hood and sword, no bracelet, setting off",
                 "OW11 ZR atop the ladder to the pillar, child with hood and sword, no bracelet, setting on",
                 RR_ZR_ATOP_LADDER, RR_ZR_PILLAR);
    // Lake Hylia front rupee (eca767dcb's Iron Boots term, absorbed by its IsChild guard).
    ResetLogic();
    Give(RG_IRON_BOOTS);
    logic->SetRandoInf(RAND_INF_CAN_SWIM, false);
    ExpectAt("OW12 LH front rupee, adult with Iron Boots, no swim", RR_LAKE_HYLIA, RC_LH_FRONT_RUPEE, true, false,
             "the child-only rupee is in logic for an adult with Iron Boots");
    logic->SetRandoInf(RAND_INF_CAN_SWIM, true);
    ExpectAt("OW13 LH front rupee, child who can swim", RR_LAKE_HYLIA, RC_LH_FRONT_RUPEE, false, true,
             "the child can no longer reach the front rupee by swimming");

    // ---- HW: RT_HW_BUNNY_CROSSING needs the trick AND the setting --------------
    ResetLogicWithBunnyHood();
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit("HW1 Wasteland crossing, child with the hood, setting on, RT_HW_BUNNY_CROSSING off",
               RR_WASTELAND_NEAR_FORTRESS, RR_HAUNTED_WASTELAND, false, false,
               "the bunny crossing is in logic with its trick off");
    randoCtx->GetTrickOption(RT_HW_BUNNY_CROSSING).Set(1);
    ExpectExit("HW2 Wasteland crossing, child with the hood, setting on, RT_HW_BUNNY_CROSSING on",
               RR_WASTELAND_NEAR_FORTRESS, RR_HAUNTED_WASTELAND, false, true,
               "Wasteland Crossing with Bunny Hood does not cross the sand (SoH eca767dcb)");
    ExpectExit("HW3 Wasteland crossing back, child with the hood, setting on, RT_HW_BUNNY_CROSSING on",
               RR_HAUNTED_WASTELAND, RR_WASTELAND_NEAR_FORTRESS, false, true,
               "Wasteland Crossing with Bunny Hood does not cross the sand back");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectExit("HW4 Wasteland crossing, child with the hood, setting off, RT_HW_BUNNY_CROSSING on",
               RR_WASTELAND_NEAR_FORTRESS, RR_HAUNTED_WASTELAND, false, false,
               "the bunny crossing trick credits the hood in a seed without Bunny Hood Effect");
    randoCtx->GetTrickOption(RT_HW_BUNNY_CROSSING).Set(0);

    // ---- WE: RT_WATER_HIGH_EMBLEM_JUMP (SoH 73a739b7a) -------------------------
    ResetLogic();
    ExpectExit("WE1 Water MQ 3F central to high emblem, adult with nothing, trick off", RR_WATER_TEMPLE_MQ_3F_CENTRAL_A,
               RR_WATER_TEMPLE_MQ_HIGH_EMBLEM, true, false, "the high-emblem jump is in logic with its trick off");
    BhlAs(true, RR_WATER_TEMPLE_MQ_3F_CENTRAL_A);
    ExpectFlag("WE2 Water3FCentralToHighEmblem, adult with nothing, trick off", logic->Water3FCentralToHighEmblem(),
               false, "the vanilla high-emblem jump is in logic with its trick off");
    randoCtx->GetTrickOption(RT_WATER_HIGH_EMBLEM_JUMP).Set(1);
    ExpectFlag("WE3 Water3FCentralToHighEmblem, adult with nothing, trick on", logic->Water3FCentralToHighEmblem(),
               true, "Water Temple Jump to High Emblem with Nothing does not credit the adult (SoH 73a739b7a)");
    logic->CurrentRegionKey = RR_NONE;
    ExpectExit("WE4 Water MQ 3F central to high emblem, adult with nothing, trick on", RR_WATER_TEMPLE_MQ_3F_CENTRAL_A,
               RR_WATER_TEMPLE_MQ_HIGH_EMBLEM, true, true,
               "Water Temple Jump to High Emblem with Nothing does not credit the adult in MQ");
    ResetLogicWithBunnyHood();
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectExit("WE5 Water MQ 3F central to high emblem, child with the hood, trick on, setting off",
               RR_WATER_TEMPLE_MQ_3F_CENTRAL_A, RR_WATER_TEMPLE_MQ_HIGH_EMBLEM, false, false,
               "the child's high-emblem jump credits the hood in a seed without Bunny Hood Effect");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit("WE6 Water MQ 3F central to high emblem, child with the hood, trick on, setting on",
               RR_WATER_TEMPLE_MQ_3F_CENTRAL_A, RR_WATER_TEMPLE_MQ_HIGH_EMBLEM, false, true,
               "the child's hood jump to the high emblem is not in logic under the trick");
    randoCtx->GetTrickOption(RT_WATER_HIGH_EMBLEM_JUMP).Set(0);
    ExpectExit("WE7 Water MQ 3F central to high emblem, child with the hood, trick off, setting on",
               RR_WATER_TEMPLE_MQ_3F_CENTRAL_A, RR_WATER_TEMPLE_MQ_HIGH_EMBLEM, false, false,
               "the child's high-emblem jump is in logic with its trick off (73a739b7a made it a trick)");
    // eca767dcb's own form credited the child under RT_UNINTUITIVE_JUMPS; 73a739b7a replaced it.
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(1);
    BhlAs(false, RR_WATER_TEMPLE_MQ_3F_CENTRAL_A);
    ExpectFlag("WE8 Water3FCentralToHighEmblem, child with the hood, RT_UNINTUITIVE_JUMPS, trick off, setting on",
               logic->Water3FCentralToHighEmblem(), false,
               "eca767dcb's Unintuitive Jumps form was ported: 73a739b7a made the jump its own trick");
    randoCtx->GetTrickOption(RT_WATER_HIGH_EMBLEM_JUMP).Set(1);
    ExpectFlag("WE9 Water3FCentralToHighEmblem, child with the hood, trick on, setting on",
               logic->Water3FCentralToHighEmblem(), true, "the child's hood jump to the high emblem is not in logic");
    randoCtx->GetTrickOption(RT_WATER_HIGH_EMBLEM_JUMP).Set(0);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    logic->CurrentRegionKey = RR_NONE;

    // ---- RH: CanRecoilHover / CanRecoilHoverFromObject (eca767dcb) ------------
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ResetLogic();
    Give(RG_HOVER_BOOTS);
    Give(RG_MEGATON_HAMMER);
    Give(RG_MASTER_SWORD);
    Give(RG_MIRROR_SHIELD);
    BhlAs(true, RR_GF_JAIL_WINDOW);
    ExpectFlag("RH1 CanRecoilHover(RECOIL_SWORD), everything, RT_HOVER_BOOST_SIMPLE off",
               logic->CanRecoilHover(RECOIL_SWORD), false, "a recoil hover is in logic with its trick off");
    ExpectFlag("RH1 CanRecoilHover(RECOIL_SWORD_AND_SHIELD), everything, trick off",
               logic->CanRecoilHover(RECOIL_SWORD_AND_SHIELD), false, "a recoil hover is in logic with its trick off");
    ExpectFlag("RH1 CanRecoilHover(RECOIL_HAMMER), everything, trick off", logic->CanRecoilHover(RECOIL_HAMMER), false,
               "a recoil hover is in logic with its trick off");
    ExpectFlag("RH1 CanRecoilHover(RECOIL_HAMMER_AND_SHIELD), everything, trick off",
               logic->CanRecoilHover(RECOIL_HAMMER_AND_SHIELD), false, "a recoil hover is in logic with its trick off");
    ExpectFlag("RH1 CanRecoilHoverFromObject(TRECOIL_SHORT), everything, trick off",
               logic->CanRecoilHoverFromObject(TRECOIL_SHORT), false, "a recoil hover is in logic with its trick off");
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(1);
    ResetLogic();
    Give(RG_HOVER_BOOTS);
    Give(RG_MEGATON_HAMMER);
    BhlAs(true, RR_GF_JAIL_WINDOW);
    ExpectFlag("RH2 CanRecoilHover(RECOIL_HAMMER), Hover Boots and hammer, no shield, trick on",
               logic->CanRecoilHover(RECOIL_HAMMER), true, "the hammer recoil hover is not in logic");
    ExpectFlag("RH3 CanRecoilHover(RECOIL_HAMMER_AND_SHIELD), Hover Boots and hammer, no shield, trick on",
               logic->CanRecoilHover(RECOIL_HAMMER_AND_SHIELD), false,
               "the shield-needing hammer recoil is in logic without a shield");
    ExpectFlag("RH4 CanRecoilHover(RECOIL_SWORD_AND_SHIELD), Hover Boots and hammer, no shield, trick on",
               logic->CanRecoilHover(RECOIL_SWORD_AND_SHIELD), true,
               "the hammer does not stand in for sword and shield (upstream's fallthrough)");
    Give(RG_MIRROR_SHIELD);
    ExpectFlag("RH5 CanRecoilHover(RECOIL_HAMMER_AND_SHIELD), Hover Boots, hammer and Mirror Shield, trick on",
               logic->CanRecoilHover(RECOIL_HAMMER_AND_SHIELD), true, "the hammer and shield recoil is not in logic");
    ResetLogic();
    Give(RG_HOVER_BOOTS);
    Give(RG_MASTER_SWORD);
    Give(RG_MIRROR_SHIELD);
    BhlAs(true, RR_GF_JAIL_WINDOW);
    ExpectFlag("RH6 CanRecoilHover(RECOIL_SWORD_AND_SHIELD), Hover Boots, sword and shield, no hammer, trick on",
               logic->CanRecoilHover(RECOIL_SWORD_AND_SHIELD), true, "the sword and shield recoil is not in logic");
    ExpectFlag("RH7 CanRecoilHover(RECOIL_HAMMER), Hover Boots, sword and shield, no hammer, trick on",
               logic->CanRecoilHover(RECOIL_HAMMER), false, "a hammer recoil is in logic with no hammer");
    logic->CurrentRegionKey = RR_NONE;
    // The call sites: the old CanHammerRecoilHover() is RECOIL_HAMMER, which needs no shield.
    ResetLogic();
    Give(RG_HOVER_BOOTS);
    Give(RG_MEGATON_HAMMER);
    ExpectExit("RH8 Forest NE courtyard doorframe to island, adult with Hover Boots and hammer, no shield, trick on",
               RR_FOREST_TEMPLE_NE_COURTYARD_DOORFRAME, RR_FOREST_TEMPLE_NE_COURTYARD_ISLAND, true, true,
               "the old CanHammerRecoilHover() call now needs a shield (mapped past RECOIL_HAMMER)");
    ExpectExit("RH9 GF jail window to the long roof, adult with Hover Boots and hammer, trick on", RR_GF_JAIL_WINDOW,
               RR_GF_LONG_ROOF, true, true, "the long-roof hammer recoil is not in logic (eca767dcb)");
    ExpectExit("RH10 GV grotto ledge to the upper stream, adult with Hover Boots and hammer, trick on",
               RR_GV_GROTTO_LEDGE, RR_GV_UPPER_STREAM, true, true,
               "eca767dcb's hammer recoil to the upper stream is not in logic");
    randoCtx->GetTrickOption(RT_HOVER_BOOST_SIMPLE).Set(0);
    ExpectExit("RH11 Forest NE courtyard doorframe to island, adult with Hover Boots and hammer, trick off",
               RR_FOREST_TEMPLE_NE_COURTYARD_DOORFRAME, RR_FOREST_TEMPLE_NE_COURTYARD_ISLAND, true, false,
               "the hammer recoil hover is in logic with its trick off");
    ExpectExit("RH12 GF jail window to the long roof, adult with Hover Boots and hammer, trick off", RR_GF_JAIL_WINDOW,
               RR_GF_LONG_ROOF, true, false, "the hammer recoil hover is in logic with its trick off");
    ExpectExit("RH13 GV grotto ledge to the upper stream, adult with Hover Boots and hammer, trick off",
               RR_GV_GROTTO_LEDGE, RR_GV_UPPER_STREAM, true, false,
               "the hammer recoil hover to the upper stream is in logic with its trick off");

    // ---- D1: CanAvoidEnemy's new distance argument leaves the body alone -------
    ResetLogic();
    Give(RG_PROGRESSIVE_BOW);
    BhlAs(true, RR_GF_JAIL_WINDOW);
    ExpectFlag("D1 CanAvoidEnemy(RE_BEAMOS, ED_CLOSE, grounded, 2), adult with only the Bow",
               logic->CanAvoidEnemy(RE_BEAMOS, ED_CLOSE, true, 2), false,
               "the Bow avoids two grounded Beamos: the body took stage 5's Beamos rewrite");
    ExpectFlag("D2 CanAvoidEnemy(RE_BEAMOS, ED_CLOSE, grounded, 1), adult with only the Bow",
               logic->CanAvoidEnemy(RE_BEAMOS, ED_CLOSE, true, 1), true,
               "the Bow no longer avoids one grounded Beamos");
    logic->CurrentRegionKey = RR_NONE;

    for (size_t i = 0; i < sizeof(kBhlForcedTricks) / sizeof(kBhlForcedTricks[0]); i++) {
        randoCtx->GetTrickOption(kBhlForcedTricks[i]).Set(savedTricks[i]);
    }
}

// ---- #1647 stage 1b (SoH eca767dcb's Masks as Adult): MA, MC, BV, AH, TR ----
// The tricks these legs force off (saved and restored).
const RandomizerTrick kMaaForcedTricks[] = {
    RT_UNINTUITIVE_JUMPS,   RT_GROUND_JUMP,    RT_GROUND_JUMP_HARD, RT_HOVER_BOOST_SIMPLE, RT_DAMAGE_BOOST_SIMPLE,
    RT_FOREST_DOORFRAME,    RT_LENS_GANON,     RT_GTG_LAVA_JUMP,    RT_SLIDE_JUMP,         RT_JABU_BOSS_HOVER,
    RT_SHADOW_MQ_HUGE_PIT,  RT_LENS_SHADOW_MQ, RT_ZR_LOWER,         RT_SPIRIT_STATUE_JUMP, RT_DC_EYES_CHU,
    RT_DC_EYES_BUNNYHOVERS,
};

/** ResetLogic, holding the Bunny Hood, with Climb withheld (the legs run under Shuffle Climb). */
void MaaResetAdultWithHood() {
    ResetLogicWithBunnyHood();
    logic->SetRandoInf(RAND_INF_CAN_CLIMB, false);
}

/** One exit for an adult holding the hood, Bunny Hood Effect on: closed with Masks as Adult off, open with it on. */
void MaaExitOffOn(const std::shared_ptr<Rando::Context>& randoCtx, const char* legOff, const char* legOn,
                  RandomizerRegion from, RandomizerRegion to) {
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectExit(legOff, from, to, true, false, "the adult's hood jump is in logic in a seed without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectExit(legOn, from, to, true, true,
               "the adult's hood jump is not in logic under Masks as Adult and Bunny Hood Effect (#1647)");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
}

/** CanUse of one mask (Mask Quest Shuffle's ownership flag): an adult with Masks as Adult off and on, a child off. */
void MaaMaskOffOnChild(const std::shared_ptr<Rando::Context>& randoCtx, const char* legOff, const char* legOn,
                       const char* legChild, RandomizerGet mask, RandomizerInf owned) {
    ResetLogic();
    logic->SetRandoInf(owned, true);
    BhlAs(true, RR_THE_LOST_WOODS);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag(legOff, logic->CanUse(mask), false,
               "logic lets the adult wear the mask in a seed without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag(legOn, logic->CanUse(mask), true,
               "logic does not let the adult wear a mask the game lets him wear under Masks as Adult (#1647)");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    BhlAs(false, RR_THE_LOST_WOODS);
    ExpectFlag(legChild, logic->CanUse(mask), true, "the child can no longer wear the mask");
    logic->CurrentRegionKey = RR_NONE;
}

/** One location for an adult: out of logic with Masks as Adult off, in logic with it on. */
void MaaCheckOffOn(const std::shared_ptr<Rando::Context>& randoCtx, const char* legOff, const char* legOn,
                   RandomizerRegion region, RandomizerCheck rc) {
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectAt(legOff, region, rc, true, false, "the adult's mask use is in logic in a seed without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectAt(legOn, region, rc, true, true, "the adult's mask use is not in logic under Masks as Adult (#1647)");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
}

/**
 * #1647, operator 2026-11-15 "Widen to all": every mask the game lets an adult wear under Masks as Adult is
 * usable by him in logic, and every use that needs the child stays the child's.
 *  MW1..MW9  the Keaton, Goron and Zora Masks (#978's cases): adult off false, adult on true, child true.
 *  SU1..SU14 the Skull Mask's and the Mask of Truth's uses: the Keese stun (CanAvoidEnemy), the Deku Theater's
 *            two rewards, the Spirit MQ big wall exit, its two wall silvers and its silver event (one pair each).
 *  BT1, BT2  the mask trade stays the child's: an adult with Masks as Adult on borrows nothing (Kakariko
 *            Village's LOGIC_BORROW_SKULL_MASK); the child does.
 *  BP1..BP17 the cross-game masks (operator 2026-11-15, "Make them adult-wearable"): with their rule frozen on,
 *            the adult is credited the Blast and Stone Masks (CanUse, BlastOrSmash, Beamos, Gerudo guards) under
 *            the seed's Masks as Adult and not without it, never without their rule, and never from the Masks
 *            Equippable as Adult enhancement or Timeless Equipment (logic reads no CVar); the child always is.
 *  BW1, BW2  the gameplay reader the masks follow, OoT_ForeignMask_AdultMayWear: off with everything off, on with
 *            the enhancement alone.
 */
void RunMasksAsAdultWideLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    MaaMaskOffOnChild(randoCtx, "MW1 CanUse(RG_KEATON_MASK), adult holding it, Masks as Adult off",
                      "MW2 CanUse(RG_KEATON_MASK), adult holding it, Masks as Adult on",
                      "MW3 CanUse(RG_KEATON_MASK), child holding it, Masks as Adult off", RG_KEATON_MASK,
                      RAND_INF_CHILD_TRADES_HAS_MASK_KEATON);
    MaaMaskOffOnChild(randoCtx, "MW4 CanUse(RG_GORON_MASK), adult holding it, Masks as Adult off",
                      "MW5 CanUse(RG_GORON_MASK), adult holding it, Masks as Adult on",
                      "MW6 CanUse(RG_GORON_MASK), child holding it, Masks as Adult off", RG_GORON_MASK,
                      RAND_INF_CHILD_TRADES_HAS_MASK_GORON);
    MaaMaskOffOnChild(randoCtx, "MW7 CanUse(RG_ZORA_MASK), adult holding it, Masks as Adult off",
                      "MW8 CanUse(RG_ZORA_MASK), adult holding it, Masks as Adult on",
                      "MW9 CanUse(RG_ZORA_MASK), child holding it, Masks as Adult off", RG_ZORA_MASK,
                      RAND_INF_CHILD_TRADES_HAS_MASK_ZORA);

    // ---- SU1..SU8: the Skull Mask's and the Mask of Truth's uses, for an adult ----
    ResetLogic();
    logic->SetRandoInf(RAND_INF_CHILD_TRADES_HAS_MASK_SKULL, true);
    BhlAs(true, RR_THE_LOST_WOODS);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag("SU1 CanAvoidEnemy(RE_KEESE), adult with only the Skull Mask, Masks as Adult off",
               logic->CanAvoidEnemy(RE_KEESE), false, "the adult's Skull Mask calms Keese without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("SU2 CanAvoidEnemy(RE_KEESE), adult with only the Skull Mask, Masks as Adult on",
               logic->CanAvoidEnemy(RE_KEESE), true,
               "the adult's Skull Mask does not calm Keese under Masks as Adult (z_en_firefly.c asks no age)");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    logic->CurrentRegionKey = RR_NONE;
    MaaCheckOffOn(randoCtx, "SU3 Deku Theater Skull Mask reward, adult with the Skull Mask, Masks as Adult off",
                  "SU4 Deku Theater Skull Mask reward, adult with the Skull Mask, Masks as Adult on", RR_DEKU_THEATER,
                  RC_DEKU_THEATER_SKULL_MASK);
    GiveClimb();
    ExpectExit("SU7 Spirit MQ big wall base to upper, adult with the Skull Mask and Climb, Masks as Adult off",
               RR_SPIRIT_TEMPLE_MQ_BIG_WALL_BASE, RR_SPIRIT_TEMPLE_MQ_BIG_WALL_UPPER, true, false,
               "the adult's Skull Mask past the Keese is in logic without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectExit("SU8 Spirit MQ big wall base to upper, adult with the Skull Mask and Climb, Masks as Adult on",
               RR_SPIRIT_TEMPLE_MQ_BIG_WALL_BASE, RR_SPIRIT_TEMPLE_MQ_BIG_WALL_UPPER, true, true,
               "the adult's Skull Mask past the Keese is not in logic under Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    MaaCheckOffOn(randoCtx,
                  "SU9 Spirit MQ highest wall silver, adult with the Skull Mask and Climb, Masks as Adult off",
                  "SU10 Spirit MQ highest wall silver, adult with the Skull Mask and Climb, Masks as Adult on",
                  RR_SPIRIT_TEMPLE_MQ_BIG_WALL_BASE, RC_SPIRIT_MQ_HIGHEST_WALL_SILVER);
    MaaCheckOffOn(randoCtx,
                  "SU11 Spirit MQ middle wall silver, adult with the Skull Mask and Climb, Masks as Adult off",
                  "SU12 Spirit MQ middle wall silver, adult with the Skull Mask and Climb, Masks as Adult on",
                  RR_SPIRIT_TEMPLE_MQ_BIG_WALL_BASE, RC_SPIRIT_MQ_MIDDLE_WALL_SILVER);
    FireEvents(RR_SPIRIT_TEMPLE_MQ_BIG_WALL_UPPER, true);
    ExpectEvent("SU13 Spirit MQ big wall silver event, adult with the Skull Mask and Climb, Masks as Adult off",
                LOGIC_SPIRIT_MQ_SILVER_BIG_WALL, false,
                "the adult's Skull Mask past the Keese collects the big wall silvers without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    FireEvents(RR_SPIRIT_TEMPLE_MQ_BIG_WALL_UPPER, true);
    ExpectEvent("SU14 Spirit MQ big wall silver event, adult with the Skull Mask and Climb, Masks as Adult on",
                LOGIC_SPIRIT_MQ_SILVER_BIG_WALL, true,
                "the adult's Skull Mask past the Keese does not collect the big wall silvers under Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ResetLogic();
    logic->SetRandoInf(RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH, true);
    logic->SetRandoInf(RAND_INF_CAN_SPEAK_DEKU, true);
    MaaCheckOffOn(randoCtx, "SU5 Deku Theater Mask of Truth reward, adult with the Mask of Truth, Masks as Adult off",
                  "SU6 Deku Theater Mask of Truth reward, adult with the Mask of Truth, Masks as Adult on",
                  RR_DEKU_THEATER, RC_DEKU_THEATER_MASK_OF_TRUTH);

    // ---- BT1, BT2: the mask trade is the child's, whatever the setting ----
    ResetLogic();
    GiveNut();
    GiveChildWallet();
    logic->Set(LOGIC_CAN_BORROW_MASKS, true);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    FireEvents(RR_KAKARIKO_VILLAGE, true);
    ExpectEvent("BT1 Kakariko borrows the Skull Mask, adult with wallet and Hylian, Masks as Adult on",
                LOGIC_BORROW_SKULL_MASK, false,
                "the adult borrows a mask from the Happy Mask Shop's child trade under Masks as Adult");
    FireEvents(RR_KAKARIKO_VILLAGE, false);
    ExpectEvent("BT2 Kakariko borrows the Skull Mask, child with wallet and Hylian, Masks as Adult on",
                LOGIC_BORROW_SKULL_MASK, true, "the child no longer borrows the Skull Mask");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);

#ifdef RSBS_SINGLE_EXECUTABLE
    // ---- BP1..BP17: the cross-game masks under Masks as Adult (their rule frozen on for these legs only) ----
    const ComboSettingsRecord savedComboSettings = gComboCtx.comboSettings;
    const ComboItemRulesRecord savedComboItemRules = gComboCtx.comboItemRules;
    if (gComboCtx.comboSettings.formatVersion == 0) {
        Combo_ComboSettingsDefaults(&gComboCtx.comboSettings);
    }
    gComboCtx.comboItemRules.crossItemFlags |= RSBS_XITEM_BLAST_MASK_OOT | RSBS_XITEM_STONE_MASK_OOT;
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ResetLogic();
    logic->SetRandoInf(RAND_INF_OBTAINED_BLAST_MASK, true);
    logic->SetRandoInf(RAND_INF_OBTAINED_STONE_MASK, true);
    BhlAs(true, RR_THE_LOST_WOODS);
    ExpectFlag("BP1 CanUse(RG_BLAST_MASK), adult holding it, its rule on, Masks as Adult on",
               logic->CanUse(RG_BLAST_MASK), true,
               "the adult is not credited the Blast Mask he can wear under Masks as Adult");
    ExpectFlag("BP2 BlastOrSmash, adult holding only the Blast Mask, its rule on, Masks as Adult on",
               logic->BlastOrSmash(), true,
               "the adult is not credited the Blast Mask's explosion he can make under Masks as Adult");
    ExpectFlag("BP3 CanUse(RG_STONE_MASK), adult holding it, its rule on, Masks as Adult on",
               logic->CanUse(RG_STONE_MASK), true,
               "the adult is not credited the Stone Mask he can wear under Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag("BP7 CanUse(RG_BLAST_MASK), adult holding it, its rule on, Masks as Adult off",
               logic->CanUse(RG_BLAST_MASK), false, "logic credits the adult the Blast Mask without Masks as Adult");
    ExpectFlag("BP8 BlastOrSmash, adult holding only the Blast Mask, its rule on, Masks as Adult off",
               logic->BlastOrSmash(), false,
               "logic credits the adult the Blast Mask's explosion without Masks as Adult");
    ExpectFlag("BP9 CanUse(RG_STONE_MASK), adult holding it, its rule on, Masks as Adult off",
               logic->CanUse(RG_STONE_MASK), false, "logic credits the adult the Stone Mask without Masks as Adult");
    // BP12, BP13: the per-install enhancement and the cheat give gameplay, never logic (cleared at once:
    // RunMasksAsAdultLegs cleared both before these legs and restores them after).
    CVarSetInteger(CVAR_ENHANCEMENT("AdultMasks"), 1);
    const bool bp12Blast = logic->CanUse(RG_BLAST_MASK);
    const bool bp12Stone = logic->CanUse(RG_STONE_MASK);
    CVarClear(CVAR_ENHANCEMENT("AdultMasks"));
    ExpectFlag("BP12 CanUse of both cross-game masks, adult, Masks as Adult off, Masks Equippable as Adult on",
               bp12Blast || bp12Stone, false, "logic reads the Masks Equippable as Adult enhancement's CVar");
    CVarSetInteger(CVAR_CHEAT("TimelessEquipment"), 1);
    const bool bp13Blast = logic->CanUse(RG_BLAST_MASK);
    const bool bp13Stone = logic->CanUse(RG_STONE_MASK);
    CVarClear(CVAR_CHEAT("TimelessEquipment"));
    ExpectFlag("BP13 CanUse of both cross-game masks, adult, Masks as Adult off, Timeless Equipment on",
               bp13Blast || bp13Stone, false, "logic reads the Timeless Equipment cheat's CVar");
    // BP10, BP11: the frozen rule stays a condition under the setting.
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    gComboCtx.comboItemRules.crossItemFlags &= ~(RSBS_XITEM_BLAST_MASK_OOT | RSBS_XITEM_STONE_MASK_OOT);
    ExpectFlag("BP10 CanUse(RG_BLAST_MASK), adult holding it, its rule off, Masks as Adult on",
               logic->CanUse(RG_BLAST_MASK), false, "Masks as Adult credits the Blast Mask without its rule");
    ExpectFlag("BP11 CanUse(RG_STONE_MASK), adult holding it, its rule off, Masks as Adult on",
               logic->CanUse(RG_STONE_MASK), false, "Masks as Adult credits the Stone Mask without its rule");
    gComboCtx.comboItemRules.crossItemFlags |= RSBS_XITEM_BLAST_MASK_OOT | RSBS_XITEM_STONE_MASK_OOT;
    // BP14..BP17: the rules that credit them, each mask held alone.
    ResetLogic();
    logic->SetRandoInf(RAND_INF_OBTAINED_BLAST_MASK, true);
    BhlAs(true, RR_THE_LOST_WOODS);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag("BP14 CanKillEnemy(RE_BEAMOS), adult holding only the Blast Mask, Masks as Adult off",
               logic->CanKillEnemy(RE_BEAMOS), false, "the adult's Blast Mask kills a Beamos without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("BP15 CanKillEnemy(RE_BEAMOS), adult holding only the Blast Mask, Masks as Adult on",
               logic->CanKillEnemy(RE_BEAMOS), true,
               "the adult's Blast Mask does not kill a Beamos under Masks as Adult (OoTMM's can_kill_beamos)");
    const uint8_t savedPassGuards = randoCtx->GetTrickOption(RT_PASS_GUARDS_WITH_NOTHING).Get();
    randoCtx->GetTrickOption(RT_PASS_GUARDS_WITH_NOTHING).Set(0);
    ResetLogic();
    logic->SetRandoInf(RAND_INF_OBTAINED_STONE_MASK, true);
    BhlAs(true, RR_THE_LOST_WOODS);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag("BP16 CanPassEnemy(RE_GERUDO_GUARD), adult holding only the Stone Mask, Masks as Adult off",
               logic->CanPassEnemy(RE_GERUDO_GUARD), false,
               "the adult's Stone Mask passes a Gerudo guard without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("BP17 CanPassEnemy(RE_GERUDO_GUARD), adult holding only the Stone Mask, Masks as Adult on",
               logic->CanPassEnemy(RE_GERUDO_GUARD), true,
               "the adult's Stone Mask does not pass a Gerudo guard under Masks as Adult (OoTMM's evade_gerudo)");
    randoCtx->GetTrickOption(RT_PASS_GUARDS_WITH_NOTHING).Set(savedPassGuards);
    // BP4..BP6: the child, both held, Masks as Adult off: unchanged, so BP7..BP9 pin the age, not the rule.
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ResetLogic();
    logic->SetRandoInf(RAND_INF_OBTAINED_BLAST_MASK, true);
    logic->SetRandoInf(RAND_INF_OBTAINED_STONE_MASK, true);
    BhlAs(false, RR_THE_LOST_WOODS);
    ExpectFlag("BP4 CanUse(RG_BLAST_MASK), child holding it, its rule on", logic->CanUse(RG_BLAST_MASK), true,
               "the child can no longer use the Blast Mask, so BP7 proves nothing");
    ExpectFlag("BP5 BlastOrSmash, child holding only the Blast Mask, its rule on", logic->BlastOrSmash(), true,
               "the child's Blast Mask no longer counts as an explosion, so BP8 proves nothing");
    ExpectFlag("BP6 CanUse(RG_STONE_MASK), child holding it, its rule on", logic->CanUse(RG_STONE_MASK), true,
               "the child can no longer use the Stone Mask, so BP9 proves nothing");
    logic->CurrentRegionKey = RR_NONE;
    gComboCtx.comboItemRules = savedComboItemRules;
    gComboCtx.comboSettings = savedComboSettings;

    // ---- BW1, BW2: the gameplay reader (ForeignMaskOoT.cpp), the setting off ----
    ExpectFlag("BW1 OoT_ForeignMask_AdultMayWear, enhancement, cheat and seed setting off",
               OoT_ForeignMask_AdultMayWear() != 0, false,
               "adult Link may wear the Blast and Stone Masks with nothing on");
    CVarSetInteger(CVAR_ENHANCEMENT("AdultMasks"), 1);
    const bool bw2 = OoT_ForeignMask_AdultMayWear() != 0;
    CVarClear(CVAR_ENHANCEMENT("AdultMasks"));
    ExpectFlag("BW2 OoT_ForeignMask_AdultMayWear, Masks Equippable as Adult on", bw2, true,
               "the enhancement does not let adult Link wear the Blast and Stone Masks (OoT_MasksEquippableAsAdult)");
#endif
}

void RunMasksAsAdultLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const char* const kAdultMasksKey = CVAR_ENHANCEMENT("AdultMasks");
    const char* const kTimelessKey = CVAR_CHEAT("TimelessEquipment");
    const bool adultMasksExisted = CVarExists(kAdultMasksKey);
    const int adultMasksValue = CVarGetInteger(kAdultMasksKey, 0);
    const bool timelessExisted = CVarExists(kTimelessKey);
    const int timelessValue = CVarGetInteger(kTimelessKey, 0);
    const uint8_t savedFortress = randoCtx->GetOption(RSK_GERUDO_FORTRESS).Get();
    const uint8_t savedShuffleClimb = randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Get();
    const u8 savedQuest = gSaveContext.ship.quest.id;
    uint8_t savedTricks[sizeof(kMaaForcedTricks) / sizeof(kMaaForcedTricks[0])];
    for (size_t i = 0; i < sizeof(kMaaForcedTricks) / sizeof(kMaaForcedTricks[0]); i++) {
        savedTricks[i] = randoCtx->GetTrickOption(kMaaForcedTricks[i]).Get();
        randoCtx->GetTrickOption(kMaaForcedTricks[i]).Set(0);
    }
    CVarClear(kAdultMasksKey);
    CVarClear(kTimelessKey);
    randoCtx->GetOption(RSK_GERUDO_FORTRESS).Set(RO_GF_CARPENTERS_NORMAL);
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(1);

    // ---- MA1..MA8: the one reader of Masks Equippable as Adult (GameExports_SingleExe.cpp) ----
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("MA1 reader, vanilla file while the rando context holds Masks as Adult on", OoT_AdultMasksEnabled() != 0,
               false, "a vanilla file reads a randomizer seed's Masks as Adult");
    ExpectFlag("MA2 masks equippable as adult, vanilla file, rando context holds the setting on",
               OoT_MasksEquippableAsAdult() != 0, false, "a vanilla file lets adult Link wear masks from the seed");
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag("MA3 reader, randomizer file with Masks as Adult off, enhancement unset", OoT_AdultMasksEnabled() != 0,
               false, "adult masks are forced with the seed setting off");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("MA4 reader, randomizer file with Masks as Adult on, enhancement unset", OoT_AdultMasksEnabled() != 0,
               true, "the seed's Masks as Adult does not force Masks Equippable as Adult (SoH eca767dcb)");
    ExpectFlag("MA5 masks equippable as adult, randomizer file with Masks as Adult on",
               OoT_MasksEquippableAsAdult() != 0, true, "adult Link cannot wear the masks the file's logic expects");
    ExpectFlag("MA5 the enhancement's CVar after the reads", CVarExists(kAdultMasksKey), false,
               "the reader stored the forced setting into the player's enhancement");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    CVarSetInteger(kAdultMasksKey, 1);
    ExpectFlag("MA6 reader, randomizer file with Masks as Adult off, enhancement on", OoT_AdultMasksEnabled() != 0,
               true, "the player's own Masks Equippable as Adult no longer works with the seed setting off");
    CVarClear(kAdultMasksKey);
    CVarSetInteger(kTimelessKey, 1);
    ExpectFlag("MA7 masks equippable as adult, Timeless Equipment alone", OoT_MasksEquippableAsAdult() != 0, true,
               "the Timeless Equipment cheat no longer lets adult Link wear masks");
    ExpectFlag("MA7 reader, Timeless Equipment alone", OoT_AdultMasksEnabled() != 0, false,
               "the cheat reads as the Masks Equippable as Adult enhancement");
    CVarClear(kTimelessKey);
    ExpectFlag("MA8 decision, enhancement off, seed setting off", OoT_AdultMasksEnabledFor(0, 0) != 0, false,
               "adult masks are on with nothing asking for them");
    ExpectFlag("MA8 decision, enhancement on, seed setting off", OoT_AdultMasksEnabledFor(1, 0) != 0, true,
               "the enhancement alone no longer turns adult masks on");
    ExpectFlag("MA8 decision, enhancement off, seed setting on", OoT_AdultMasksEnabledFor(0, 1) != 0, true,
               "the seed setting alone does not turn adult masks on");
    gSaveContext.ship.quest.id = savedQuest;

    // ---- MC1..MC7: Logic::CanUse of the masks and Logic::BunnyHood (logic.cpp) ----
    MaaResetAdultWithHood();
    BhlAs(true, RR_THE_LOST_WOODS);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag("MC1 CanUse(RG_BUNNY_HOOD), adult holding the hood, Masks as Adult off", logic->CanUse(RG_BUNNY_HOOD),
               false, "logic lets the adult wear the hood in a seed without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("MC2 CanUse(RG_BUNNY_HOOD), adult holding the hood, Masks as Adult on", logic->CanUse(RG_BUNNY_HOOD),
               true, "logic does not let the adult wear the hood under Masks as Adult (SoH eca767dcb)");
    logic->SetRandoInf(RAND_INF_CHILD_TRADES_HAS_MASK_SKULL, true);
    ExpectFlag("MC3 CanUse(RG_SKULL_MASK), adult holding it, Masks as Adult on", logic->CanUse(RG_SKULL_MASK), true,
               "upstream's Masks as Adult case no longer covers the Skull Mask");
    logic->SetRandoInf(RAND_INF_CHILD_TRADES_HAS_MASK_KEATON, true);
    ExpectFlag("MC4 CanUse(RG_KEATON_MASK), adult holding it, Masks as Adult on", logic->CanUse(RG_KEATON_MASK), true,
               "Masks as Adult does not cover the Keaton Mask, which the game lets an adult wear (#1647, all masks)");
    ExpectFlag("MC5 Logic::BunnyHood, adult holding the hood, both settings on", logic->BunnyHood(), true,
               "logic does not credit the adult's hood jump under Masks as Adult and Bunny Hood Effect");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectFlag("MC6 Logic::BunnyHood, adult holding the hood, Masks as Adult on, Bunny Hood Effect off",
               logic->BunnyHood(), false, "Masks as Adult alone credits the hood's jump");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    BhlAs(false, RR_THE_LOST_WOODS);
    ExpectFlag("MC7 CanUse(RG_BUNNY_HOOD), child holding the hood, Masks as Adult off", logic->CanUse(RG_BUNNY_HOOD),
               true, "the child can no longer wear the hood");
    logic->CurrentRegionKey = RR_NONE;

    // ---- MW1..MW9: the masks #978 gave a case, widened to all (operator 2026-11-15) ----
    RunMasksAsAdultWideLegs(randoCtx);

    // ---- BV1..BV5: Logic::BunnyHovers (Hover Boots and the hood, both settings) ----
    MaaResetAdultWithHood();
    Give(RG_HOVER_BOOTS);
    BhlAs(true, RR_THE_LOST_WOODS);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("BV1 BunnyHovers, adult with Hover Boots and the hood, both settings on", logic->BunnyHovers(), true,
               "Hover Boots and the hood do not hover with the hood's speed");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    ExpectFlag("BV2 BunnyHovers, adult with Hover Boots and the hood, Masks as Adult off", logic->BunnyHovers(), false,
               "the bunny hover is in logic without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectFlag("BV3 BunnyHovers, adult with Hover Boots and the hood, Bunny Hood Effect off", logic->BunnyHovers(),
               false, "the bunny hover is in logic without Bunny Hood Effect");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    BhlAs(false, RR_THE_LOST_WOODS);
    ExpectFlag("BV4 BunnyHovers, child with Hover Boots and the hood, both settings on", logic->BunnyHovers(), false,
               "the child is credited Hover Boots it cannot wear");
    MaaResetAdultWithHood();
    BhlAs(true, RR_THE_LOST_WOODS);
    ExpectFlag("BV5 BunnyHovers, adult with the hood but no Hover Boots, both settings on", logic->BunnyHovers(), false,
               "the bunny hover is in logic without Hover Boots");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    logic->CurrentRegionKey = RR_NONE;

    // ---- AH: one pair per area, an adult holding the hood, Bunny Hood Effect on ----
    MaaResetAdultWithHood();
    MaaExitOffOn(randoCtx, "AH1 DC MQ behind the mouth to behind the fire, adult with the hood, Masks as Adult off",
                 "AH2 DC MQ behind the mouth to behind the fire, adult with the hood, Masks as Adult on",
                 RR_DODONGOS_CAVERN_MQ_BEHIND_MOUTH, RR_DODONGOS_CAVERN_MQ_BACK_BEHIND_FIRE);
    MaaExitOffOn(randoCtx, "AH3 Fire maze to its platforms, adult with the hood, Masks as Adult off",
                 "AH4 Fire maze to its platforms, adult with the hood, Masks as Adult on",
                 RR_FIRE_TEMPLE_FIRE_MAZE_MAIN, RR_FIRE_TEMPLE_FIRE_MAZE_PLATFORMS);
    MaaExitOffOn(randoCtx, "AH5 Water 3F central to the entrance ledge, adult with the hood, Masks as Adult off",
                 "AH6 Water 3F central to the entrance ledge, adult with the hood, Masks as Adult on",
                 RR_WATER_TEMPLE_3F_CENTRAL_A, RR_WATER_TEMPLE_ENTRANCE_LEDGE);
    MaaExitOffOn(randoCtx, "AH7 Spirit MQ Anubis bridge chest to the grave, adult with the hood, Masks as Adult off",
                 "AH8 Spirit MQ Anubis bridge chest to the grave, adult with the hood, Masks as Adult on",
                 RR_SPIRIT_TEMPLE_MQ_ANUBIS_BRIDGE_CHEST, RR_SPIRIT_TEMPLE_MQ_ANUBIS_BRIDGE_GRAVE);
    MaaExitOffOn(randoCtx, "AH9 GF near the GS to the long roof, adult with the hood, Masks as Adult off",
                 "AH10 GF near the GS to the long roof, adult with the hood, Masks as Adult on", RR_GF_NEAR_GS,
                 RR_GF_LONG_ROOF);
    MaaExitOffOn(randoCtx, "AH11 Zora's River to the pillar, adult with the hood, Masks as Adult off",
                 "AH12 Zora's River to the pillar, adult with the hood, Masks as Adult on", RR_ZORAS_RIVER,
                 RR_ZR_PILLAR);
    // Sacred Forest Meadow above the maze: the hood rides RT_UNINTUITIVE_JUMPS; no Climb for the ladder.
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(1);
    MaaExitOffOn(randoCtx, "AH13 SFM to above the maze, adult with the hood, RT_UNINTUITIVE_JUMPS, Masks as Adult off",
                 "AH14 SFM to above the maze, adult with the hood, RT_UNINTUITIVE_JUMPS, Masks as Adult on",
                 RR_SACRED_FOREST_MEADOW, RR_SFM_ABOVE_MAZE);
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);
    // A jumpslash: Ganon's Shadow Trial chest platform to the lower switch.
    MaaResetAdultWithHood();
    Give(RG_MASTER_SWORD);
    MaaExitOffOn(randoCtx, "AH15 Ganon's Shadow Trial chest platform to lower switch, adult with hood and sword, off",
                 "AH16 Ganon's Shadow Trial chest platform to lower switch, adult with hood and sword, on",
                 RR_GANONS_CASTLE_SHADOW_TRIAL_CHEST_PLATFORM, RR_GANONS_CASTLE_SHADOW_TRIAL_LOWER_SWITCH);
    // The Spirit Temple's reverse access (location_access.cpp spiritLogicData) and SpiritWestToSkull (logic.cpp).
    BhlAs(true, RR_SPIRIT_TEMPLE_GS_LEDGE);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectFlag("AH17 SpiritWestToSkull, adult with hood and sword, Masks as Adult off", logic->SpiritWestToSkull(),
               false, "the adult's hood jump to the GS ledge is in logic without Masks as Adult");
    ExpectFlag("AH17 Spirit GS ledge reverse access, adult with hood and sword, Masks as Adult off",
               Region::spiritLogicData.at(RR_SPIRIT_TEMPLE_GS_LEDGE).reverseAccess(), false,
               "the adult's hood jump to the GS ledge from the boss door is in logic without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectFlag("AH18 SpiritWestToSkull, adult with hood and sword, Masks as Adult on", logic->SpiritWestToSkull(), true,
               "the adult's hood jump to the GS ledge is not in logic (SoH eca767dcb)");
    ExpectFlag("AH18 Spirit GS ledge reverse access, adult with hood and sword, Masks as Adult on",
               Region::spiritLogicData.at(RR_SPIRIT_TEMPLE_GS_LEDGE).reverseAccess(), true,
               "the adult's hood jump to the GS ledge from the boss door is not in logic (SoH eca767dcb)");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    logic->CurrentRegionKey = RR_NONE;
    // The Bow: GTG MQ torch side to ledge side.
    MaaResetAdultWithHood();
    Give(RG_PROGRESSIVE_BOW);
    MaaExitOffOn(randoCtx, "AH19 GTG MQ torch side to ledge side, adult with the Bow and the hood, Masks as Adult off",
                 "AH20 GTG MQ torch side to ledge side, adult with the Bow and the hood, Masks as Adult on",
                 RR_GERUDO_TRAINING_GROUND_MQ_TORCH_SIDE_PLATFORMS, RR_GERUDO_TRAINING_GROUND_MQ_LEDGE_SIDE_PLATFORMS);
    // Hover Boots and the hood (BunnyHovers).
    MaaResetAdultWithHood();
    Give(RG_HOVER_BOOTS);
    MaaExitOffOn(randoCtx,
                 "AH21 Shadow MQ upper to lower huge pit, adult with Hover Boots and hood, Masks as Adult off",
                 "AH22 Shadow MQ upper to lower huge pit, adult with Hover Boots and hood, Masks as Adult on",
                 RR_SHADOW_TEMPLE_MQ_UPPER_HUGE_PIT, RR_SHADOW_TEMPLE_MQ_LOWER_HUGE_PIT);
    randoCtx->GetTrickOption(RT_FOREST_DOORFRAME).Set(1);
    MaaExitOffOn(randoCtx, "AH23 Forest NE courtyard upper to doorframe, adult, Hover Boots, hood, no sword, off",
                 "AH24 Forest NE courtyard upper to doorframe, adult, Hover Boots, hood, no sword, on",
                 RR_FOREST_TEMPLE_NE_COURTYARD_UPPER, RR_FOREST_TEMPLE_NE_COURTYARD_DOORFRAME);
    randoCtx->GetTrickOption(RT_FOREST_DOORFRAME).Set(0);
    GiveClimb();
    MaaExitOffOn(randoCtx, "AH25 GV fortress side to the valley, adult with Hover Boots, hood and Climb, off",
                 "AH26 GV fortress side to the valley, adult with Hover Boots, hood and Climb, on", RR_GV_FORTRESS_SIDE,
                 RR_GERUDO_VALLEY);
    GiveGrab();
    MaaExitOffOn(randoCtx, "AH27 Jabu lift room to near the boss, adult with Hover Boots, hood and grab, off",
                 "AH28 Jabu lift room to near the boss, adult with Hover Boots, hood and grab, on",
                 RR_JABU_JABUS_BELLY_LIFT_ROOM, RR_JABU_JABUS_BELLY_NEAR_BOSS_ROOM);
    // Ice Cavern MQ scarecrow room: the blue fire event.
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    FireEvents(RR_ICE_CAVERN_MQ_SCARECROW_ROOM, true);
    ExpectEvent("AH29 Ice MQ scarecrow room blue fire, adult with Hover Boots and hood, Masks as Adult off",
                LOGIC_BLUE_FIRE_ACCESS, false, "the bunny hover to the blue fire is in logic without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    FireEvents(RR_ICE_CAVERN_MQ_SCARECROW_ROOM, true);
    ExpectEvent("AH30 Ice MQ scarecrow room blue fire, adult with Hover Boots and hood, Masks as Adult on",
                LOGIC_BLUE_FIRE_ACCESS, true, "the bunny hover to the blue fire is not in logic (SoH eca767dcb)");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    // Shadow MQ spikes door platform: the east target rupee by bunny hover and jumpslash, no Longshot.
    MaaResetAdultWithHood();
    Give(RG_HOVER_BOOTS);
    Give(RG_MASTER_SWORD);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectAt("AH31 Shadow MQ door platform east target rupee, adult, Hover Boots, hood, sword, Masks as Adult off",
             RR_SHADOW_TEMPLE_MQ_SPIKES_DOOR_PLATFORM, RC_SHADOW_MQ_E_TARGET_SPIKES_SILVER, true, false,
             "the bunny hover to the east target rupee is in logic without Masks as Adult");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    ExpectAt("AH32 Shadow MQ door platform east target rupee, adult, Hover Boots, hood, sword, Masks as Adult on",
             RR_SHADOW_TEMPLE_MQ_SPIKES_DOOR_PLATFORM, RC_SHADOW_MQ_E_TARGET_SPIKES_SILVER, true, true,
             "the bunny hover to the east target rupee is not in logic (SoH eca767dcb)");
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);

    // ---- TR1..TR4: RT_DC_EYES_BUNNYHOVERS (SoH eca767dcb) -----------------------
    MaaResetAdultWithHood();
    Give(RG_HOVER_BOOTS);
    GiveGoronsBracelet();
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_ON);
    FireEvents(RR_DODONGOS_CAVERN_LOBBY, true);
    ExpectEvent("TR1 DC lobby eyes, adult with Hover Boots, hood and bracelet, both settings on, trick off",
                LOGIC_DC_EYES_LIT, false, "the bunnyhover bomb flower lights the eyes with its trick off");
    randoCtx->GetTrickOption(RT_DC_EYES_BUNNYHOVERS).Set(1);
    FireEvents(RR_DODONGOS_CAVERN_LOBBY, true);
    ExpectEvent("TR2 DC lobby eyes, adult with Hover Boots, hood and bracelet, both settings on, trick on",
                LOGIC_DC_EYES_LIT, true,
                "Dodongo's Cavern Dodongo with Goron Bracelet and Bunnyhovers does not light the eyes");
    MaaResetAdultWithHood();
    Give(RG_HOVER_BOOTS);
    GiveGoronsBracelet();
    FireEvents(RR_DODONGOS_CAVERN_MQ_LOBBY, true);
    ExpectEvent("TR3 DC MQ lobby eyes, adult with Hover Boots, hood and bracelet, both settings on, trick on",
                LOGIC_DC_EYES_LIT, true, "the bunnyhover trick does not light the MQ eyes");
    MaaResetAdultWithHood();
    Give(RG_HOVER_BOOTS);
    GiveGoronsBracelet();
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    FireEvents(RR_DODONGOS_CAVERN_LOBBY, true);
    ExpectEvent("TR4 DC lobby eyes, adult with Hover Boots, hood and bracelet, trick on, Masks as Adult off",
                LOGIC_DC_EYES_LIT, false, "the bunnyhover trick lights the eyes without Masks as Adult");
    randoCtx->GetTrickOption(RT_DC_EYES_BUNNYHOVERS).Set(0);

    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    randoCtx->GetOption(RSK_GERUDO_FORTRESS).Set(savedFortress);
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(savedShuffleClimb);
    for (size_t i = 0; i < sizeof(kMaaForcedTricks) / sizeof(kMaaForcedTricks[0]); i++) {
        randoCtx->GetTrickOption(kMaaForcedTricks[i]).Set(savedTricks[i]);
    }
    CVarClear(kAdultMasksKey);
    if (adultMasksExisted) {
        CVarSetInteger(kAdultMasksKey, adultMasksValue);
    }
    CVarClear(kTimelessKey);
    if (timelessExisted) {
        CVarSetInteger(kTimelessKey, timelessValue);
    }
}

void RunBunnyHoodLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const char* const kEffectKey = CVAR_ENHANCEMENT("MMBunnyHood");
    const uint8_t savedBunnyHood = randoCtx->GetOption(RSK_BUNNY_HOOD).Get();
    // #1647 stage 1b: every leg before the MA legs is about Masks as Adult off.
    const uint8_t savedMasksAsAdult = randoCtx->GetOption(RSK_MASKS_AS_ADULT).Get();
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    const uint8_t savedMaskQuest = randoCtx->GetOption(RSK_MASK_QUEST).Get();
    const uint8_t savedLwBridge = randoCtx->GetTrickOption(RT_LW_BRIDGE).Get();
    const u8 savedQuest = gSaveContext.ship.quest.id;
    const bool effectExisted = CVarExists(kEffectKey);
    const int effectValue = CVarGetInteger(kEffectKey, BUNNY_HOOD_VANILLA);
    CVarClear(kEffectKey);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(RO_MASK_QUEST_SHUFFLE);
    randoCtx->GetTrickOption(RT_LW_BRIDGE).Set(0);

    // ---- BH1..BH4: the one reader (GameExports_SingleExe.cpp) -----------------
    // The shared Bunny Hood rule is whatever this process froze (nothing), so the
    // "off" legs ask only that the seed setting does not force the longer jump.
    gSaveContext.ship.quest.id = QUEST_NORMAL;
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectFlag("BH1 reader, vanilla file while the rando context holds Bunny Hood Effect on",
               OoT_BunnyHoodEffectMode() == BUNNY_HOOD_FAST_AND_JUMP, false,
               "a vanilla file reads a randomizer seed's Bunny Hood Effect");
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectFlag("BH2 reader, randomizer file with Bunny Hood Effect off",
               OoT_BunnyHoodEffectMode() == BUNNY_HOOD_FAST_AND_JUMP, false,
               "the longer jump is forced with the seed setting off");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectMode("BH3 reader, randomizer file with Bunny Hood Effect on, enhancement unset", OoT_BunnyHoodEffectMode(),
               BUNNY_HOOD_FAST_AND_JUMP, "the seed's Bunny Hood Effect does not force Faster + Longer Jump");
    ExpectFlag("BH3 the enhancement's CVar after the reads", CVarExists(kEffectKey), false,
               "the reader stored the forced effect into the player's enhancement");
    CVarSetInteger(kEffectKey, BUNNY_HOOD_FAST);
    ExpectMode("BH4 reader, randomizer file with Bunny Hood Effect on, enhancement Faster Run",
               OoT_BunnyHoodEffectMode(), BUNNY_HOOD_FAST_AND_JUMP,
               "the player's weaker enhancement wins over the seed setting the logic relies on");
    CVarClear(kEffectKey);

    // ---- BH5..BH8: the reader's decision on its inputs ---------------------------
    ExpectMode("BH5 decision, enhancement vanilla, seed setting off, shared rule on",
               OoT_BunnyHoodEffectModeFor(BUNNY_HOOD_VANILLA, 0, 1), BUNNY_HOOD_FAST,
               "the shared Bunny Hood rule no longer raises the hood to Faster Run (#969)");
    ExpectMode("BH6 decision, enhancement Faster + Longer Jump, seed setting off, shared rule on",
               OoT_BunnyHoodEffectModeFor(BUNNY_HOOD_FAST_AND_JUMP, 0, 1), BUNNY_HOOD_FAST_AND_JUMP,
               "the player's stronger enhancement does not stand under the shared rule");
    ExpectMode("BH7 decision, enhancement vanilla, seed setting off, shared rule off",
               OoT_BunnyHoodEffectModeFor(BUNNY_HOOD_VANILLA, 0, 0), BUNNY_HOOD_VANILLA,
               "the hood is boosted with nothing asking for it");
    ExpectMode("BH8 decision, enhancement vanilla, seed setting on, shared rule off",
               OoT_BunnyHoodEffectModeFor(BUNNY_HOOD_VANILLA, 1, 0), BUNNY_HOOD_FAST_AND_JUMP,
               "the seed setting does not force Faster + Longer Jump");
    gSaveContext.ship.quest.id = savedQuest;

    // ---- BH9..BH11: Logic::BunnyHood, the setting AND CanUse(RG_BUNNY_HOOD) -------
    ResetLogicWithBunnyHood();
    logic->IsChild = true;
    logic->IsAdult = false;
    logic->CurrentRegionKey = RR_THE_LOST_WOODS;
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectFlag("BH9 Logic::BunnyHood, child holding the hood, Bunny Hood Effect off", logic->BunnyHood(), false,
               "logic credits the hood's jump in a seed without Bunny Hood Effect");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectFlag("BH10 Logic::BunnyHood, child holding the hood, Bunny Hood Effect on", logic->BunnyHood(), true,
               "logic does not credit the hood's jump under Bunny Hood Effect");
    logic->IsChild = false;
    logic->IsAdult = true;
    ExpectFlag("BH11 Logic::BunnyHood, adult holding the hood, Bunny Hood Effect on", logic->BunnyHood(), false,
               "logic credits the hood to an adult, who cannot wear it");
    logic->CurrentRegionKey = RR_NONE;

    // ---- LW1..LW4: the Lost Woods bridge jump (lost_woods.cpp, SoH e7f1801f0) -----
    ResetLogicWithBunnyHood();
    GiveKokiriSword();
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    ExpectExit("LW1 Lost Woods to the bridge, child with the hood and a sword, Bunny Hood Effect off",
               RR_THE_LOST_WOODS, RR_LW_BRIDGE, false, false,
               "the bridge jump is in logic in a seed without Bunny Hood Effect");
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit("LW2 Lost Woods to the bridge, child with the hood and a sword, Bunny Hood Effect on", RR_THE_LOST_WOODS,
               RR_LW_BRIDGE, false, true,
               "the child's bunny jump and jumpslash to the bridge is not in logic (SoH e7f1801f0)");
    ResetLogicWithBunnyHood();
    ExpectExit("LW3 Lost Woods to the bridge, child with the hood but no jumpslash, Bunny Hood Effect on",
               RR_THE_LOST_WOODS, RR_LW_BRIDGE, false, false,
               "the child reaches the bridge with the hood alone: the jump needs the jumpslash (SoH #7488)");
    ExpectExit("LW4 Lost Woods to the bridge, adult with the hood and nothing else, Bunny Hood Effect on",
               RR_THE_LOST_WOODS, RR_LW_BRIDGE, true, false,
               "the adult reaches the bridge through the hood, which only the child can wear");

    // ---- BL, OW, HW, WE, RH, D1: the rest of eca767dcb's terms (#1647) ---------
    RunBunnyHoodLogicLegs(randoCtx);
    // ---- MA, MC, BV, AH, TR: Masks as Adult and the adult hood terms (#1647 stage 1b) ----
    RunMasksAsAdultLegs(randoCtx);

    CVarClear(kEffectKey);
    if (effectExisted) {
        CVarSetInteger(kEffectKey, effectValue);
    }
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(savedBunnyHood);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(savedMasksAsAdult);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(savedMaskQuest);
    randoCtx->GetTrickOption(RT_LW_BRIDGE).Set(savedLwBridge);
}

// ---- #1647 O28: SoH 7522f31cc's crater and its follow-ups (DX..DE) ------------
// The tricks these legs force off (saved and restored), so a trick the context
// holds on cannot open a branch a leg expects closed.
const RandomizerTrick kDmcForcedTricks[] = {
    RT_FEWER_TUNIC_REQUIREMENTS, RT_UNINTUITIVE_JUMPS, RT_HOOKSHOT_LADDERS, RT_DMC_HOVER_BEAN_POH,
    RT_DMC_BOLERO_JUMP,          RT_DMC_BOULDER_JS,    RT_DMC_BOULDER_SKIP,
};

/** One exit of the crater at its heart threshold, for an adult with the Hover Boots (DT). */
struct DmcExitThreshold {
    RandomizerRegion from;
    RandomizerRegion to;
    uint8_t hearts; // 0: open with one heart, nothing to close
    bool scarecrow; // the far platform's branches also need the Longshot and the scarecrow
};

// Every exit of the seven entry regions (death_mountain_crater.cpp). With the Hover
// Boots DMCUpperToPots, DMCPotsToPad and DMCPadToPots hold and DMCUpperToPad
// (RT_UNINTUITIVE_JUMPS) does not, so each row's threshold is that of the branch
// the boots open; Shuffle Climb is off, so CanClimbLadder and Climb hold.
const DmcExitThreshold kDmcExitThresholds[] = {
    { RR_DMC_UPPER_ENTRY, RR_DMC_CRATE, 0, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_ROCK_GROTTO, 2, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_CRACKED_WALL, 3, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_SCRUB, 3, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_BLOCKED_EXIT, 5, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_POTS, 6, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_POT_GROTTO_EXIT, 6, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_CENTRAL, 12, false },
    { RR_DMC_UPPER_ENTRY, RR_DMC_FAR_PLATFORM, 14, true },
    { RR_DMC_UPPER_ENTRY, RR_DMC_TEMPLE_EXIT, 14, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_CRATE, 2, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_ROCK_GROTTO, 0, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_CRACKED_WALL, 3, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_SCRUB, 3, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_BLOCKED_EXIT, 6, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_POTS, 6, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_POT_GROTTO_EXIT, 8, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_CENTRAL, 12, false },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_FAR_PLATFORM, 14, true },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RR_DMC_TEMPLE_EXIT, 12, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_CRATE, 5, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_ROCK_GROTTO, 5, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_CRACKED_WALL, 6, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_SCRUB, 2, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_BLOCKED_EXIT, 0, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_POTS, 2, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_POT_GROTTO_EXIT, 3, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_CENTRAL, 8, false },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_FAR_PLATFORM, 9, true },
    { RR_DMC_BLOCKED_ENTRY, RR_DMC_TEMPLE_EXIT, 9, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_CRATE, 6, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_ROCK_GROTTO, 6, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_CRACKED_WALL, 6, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_SCRUB, 3, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_BLOCKED_EXIT, 2, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_POTS, 0, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_POT_GROTTO_EXIT, 2, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_CENTRAL, 6, false },
    { RR_DMC_POTS_ENTRY, RR_DMC_FAR_PLATFORM, 8, true },
    { RR_DMC_POTS_ENTRY, RR_DMC_TEMPLE_EXIT, 8, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_CRATE, 6, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_ROCK_GROTTO, 6, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_CRACKED_WALL, 8, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_SCRUB, 5, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_BLOCKED_EXIT, 3, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_POTS, 2, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_POT_GROTTO_EXIT, 0, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_CENTRAL, 6, false },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_FAR_PLATFORM, 8, true },
    { RR_DMC_POT_GROTTO_ENTRY, RR_DMC_TEMPLE_EXIT, 8, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_CRATE, 8, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_ROCK_GROTTO, 8, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_CRACKED_WALL, 9, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_SCRUB, 5, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_BLOCKED_EXIT, 5, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_POTS, 3, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_POT_GROTTO_EXIT, 3, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_CENTRAL, 3, false },
    { RR_DMC_PAD_ENTRY, RR_DMC_FAR_PLATFORM, 3, true },
    { RR_DMC_PAD_ENTRY, RR_DMC_TEMPLE_EXIT, 3, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_CRATE, 14, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_ROCK_GROTTO, 14, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_CRACKED_WALL, 15, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_SCRUB, 11, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_BLOCKED_EXIT, 11, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_POTS, 9, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_POT_GROTTO_EXIT, 9, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_CENTRAL, 9, false },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_FAR_PLATFORM, 9, true },
    { RR_DMC_TEMPLE_ENTRY, RR_DMC_TEMPLE_EXIT, 0, false },
};

/** One Piece of Heart from one entry region at its heart threshold (DT). */
struct DmcCheckThreshold {
    RandomizerRegion region;
    RandomizerCheck check;
    uint8_t hearts;
};

// The wall PoH with Climb (Shuffle Climb off) and the Hover Boots; the volcano PoH
// under RT_DMC_HOVER_BEAN_POH with the Hover Boots (DMCPadToPots holds).
const DmcCheckThreshold kDmcWallThresholds[] = {
    { RR_DMC_UPPER_ENTRY, RC_DMC_WALL_FREESTANDING_POH, 3 },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RC_DMC_WALL_FREESTANDING_POH, 2 },
    { RR_DMC_BLOCKED_ENTRY, RC_DMC_WALL_FREESTANDING_POH, 5 },
    { RR_DMC_POTS_ENTRY, RC_DMC_WALL_FREESTANDING_POH, 8 },
    { RR_DMC_POT_GROTTO_ENTRY, RC_DMC_WALL_FREESTANDING_POH, 8 },
    { RR_DMC_PAD_ENTRY, RC_DMC_WALL_FREESTANDING_POH, 8 },
    { RR_DMC_TEMPLE_ENTRY, RC_DMC_WALL_FREESTANDING_POH, 14 },
};
const DmcCheckThreshold kDmcVolcanoThresholds[] = {
    { RR_DMC_UPPER_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, 5 },
    { RR_DMC_ROCKS_GROTTO_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, 5 },
    { RR_DMC_BLOCKED_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, 3 },
    { RR_DMC_POTS_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, 2 },
    { RR_DMC_POT_GROTTO_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, 3 },
    { RR_DMC_PAD_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, 5 },
    { RR_DMC_TEMPLE_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, 11 },
};

/** ResetLogic with `hearts` heart containers (Logic::Reset seeds health from Starting Hearts). */
void DmcResetHearts(const std::shared_ptr<Rando::Context>& randoCtx, uint8_t hearts) {
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(hearts - 1);
    ResetLogic();
}

/** The Longshot and both halves of the scarecrow's song: ReachDistantScarecrow. */
void DmcGiveDistantScarecrow() {
    GiveHookshot(true);
    logic->Set(LOGIC_CHILD_SCARECROW, true);
    logic->Set(LOGIC_ADULT_SCARECROW, true);
}

/** The adult the DT rows assume, with `hearts` hearts. */
void DmcResetHoverAdult(const std::shared_ptr<Rando::Context>& randoCtx, uint8_t hearts, bool scarecrow) {
    DmcResetHearts(randoCtx, hearts);
    Give(RG_HOVER_BOOTS);
    if (scarecrow) {
        DmcGiveDistantScarecrow();
    }
}

void DmcExpectExitAt(const std::shared_ptr<Rando::Context>& randoCtx, const DmcExitThreshold& row) {
    const uint8_t open = row.hearts == 0 ? 1 : row.hearts;
    char leg[192];
    DmcResetHoverAdult(randoCtx, open, row.scarecrow);
    snprintf(leg, sizeof(leg), "DT %s -> %s, adult with Hover Boots%s, %d heart(s)",
             RegionTable(row.from)->regionName.c_str(), RegionTable(row.to)->regionName.c_str(),
             row.scarecrow ? ", Longshot and scarecrow" : "", (int)open);
    ExpectExit(leg, row.from, row.to, true, true, "the exit stays closed at its heart threshold (7522f31cc)");
    if (row.hearts > 1) {
        DmcResetHoverAdult(randoCtx, row.hearts - 1, row.scarecrow);
        snprintf(leg, sizeof(leg), "DT %s -> %s, adult with Hover Boots%s, %d heart(s)",
                 RegionTable(row.from)->regionName.c_str(), RegionTable(row.to)->regionName.c_str(),
                 row.scarecrow ? ", Longshot and scarecrow" : "", (int)row.hearts - 1);
        ExpectExit(leg, row.from, row.to, true, false, "the exit opens one heart below its threshold (7522f31cc)");
    }
}

void DmcExpectCheckAt(const std::shared_ptr<Rando::Context>& randoCtx, const DmcCheckThreshold& row) {
    char leg[192];
    for (int below = 0; below < 2; below++) {
        const uint8_t hearts = row.hearts - below;
        DmcResetHoverAdult(randoCtx, hearts, false);
        snprintf(leg, sizeof(leg), "DT %s from %s, adult with Hover Boots, %d heart(s)",
                 Rando::StaticData::GetLocation(row.check)->GetName().c_str(),
                 RegionTable(row.region)->regionName.c_str(), (int)hearts);
        ExpectAt(leg, row.region, row.check, true, below == 0,
                 below == 0 ? "the Piece of Heart is out of logic at its heart threshold (7522f31cc)"
                            : "the Piece of Heart is in logic one heart below its threshold (7522f31cc)");
    }
}

/** The exit of `from` that leads to `to`, or none. */
const Rando::Entrance* DmcFindExit(RandomizerRegion from, RandomizerRegion to) {
    for (const Rando::Entrance& exit : RegionTable(from)->exits) {
        if (exit.GetConnectedRegionKey() == to) {
            return &exit;
        }
    }
    return nullptr;
}

/** One row of entrance.cpp's table the refactor moved (DE). */
struct DmcEntranceRow {
    Rando::EntranceType type;
    RandomizerRegion parent;
    RandomizerRegion connected;
    int16_t index;
    RandomizerRegion reverseParent; // RR_NONE: NO_RETURN_ENTRANCE
    RandomizerRegion reverseConnected;
    int16_t reverseIndex;
};

// Upstream develop's rows (soh/soh/Enhancements/randomizer/entrance.cpp), with
// eca767dcb's Fire Temple reverse (RR_DMC_TEMPLE_ENTRY).
const DmcEntranceRow kDmcEntranceRows[] = {
    { Rando::EntranceType::Dungeon, RR_DMC_TEMPLE_EXIT, RR_FIRE_TEMPLE_ENTRYWAY, ENTR_FIRE_TEMPLE_ENTRANCE,
      RR_FIRE_TEMPLE_ENTRYWAY, RR_DMC_TEMPLE_ENTRY, ENTR_DEATH_MOUNTAIN_CRATER_OUTSIDE_TEMPLE },
    { Rando::EntranceType::Interior, RR_DMC_BLOCKED_EXIT, RR_DMC_GREAT_FAIRY_FOUNTAIN,
      ENTR_GREAT_FAIRYS_FOUNTAIN_MAGIC_DMC, RR_DMC_GREAT_FAIRY_FOUNTAIN, RR_DMC_BLOCKED_ENTRY,
      ENTR_DEATH_MOUNTAIN_CRATER_GREAT_FAIRY_EXIT },
    { Rando::EntranceType::GrottoGrave, RR_DMC_POT_GROTTO_EXIT, RR_DMC_SCRUB_GROTTO,
      ENTRANCE_GROTTO_LOAD(GROTTO_DMC_HAMMER_OFFSET), RR_DMC_SCRUB_GROTTO, RR_DMC_POT_GROTTO_ENTRY,
      ENTRANCE_GROTTO_EXIT(GROTTO_DMC_HAMMER_OFFSET) },
    { Rando::EntranceType::GrottoGrave, RR_DMC_ROCK_GROTTO, RR_DMC_UPPER_GROTTO,
      ENTRANCE_GROTTO_LOAD(GROTTO_DMC_UPPER_OFFSET), RR_DMC_UPPER_GROTTO, RR_DMC_ROCKS_GROTTO_ENTRY,
      ENTRANCE_GROTTO_EXIT(GROTTO_DMC_UPPER_OFFSET) },
    { Rando::EntranceType::Overworld, RR_GC_DARUNIAS_CHAMBER, RR_DMC_POTS_ENTRY, ENTR_DEATH_MOUNTAIN_CRATER_GC_EXIT,
      RR_DMC_POTS, RR_GC_DARUNIAS_CHAMBER, ENTR_GORON_CITY_DARUNIA_ROOM_EXIT },
    { Rando::EntranceType::Overworld, RR_DEATH_MOUNTAIN_SUMMIT, RR_DMC_UPPER_ENTRY,
      ENTR_DEATH_MOUNTAIN_CRATER_UPPER_EXIT, RR_DMC_CRATE, RR_DEATH_MOUNTAIN_SUMMIT,
      ENTR_DEATH_MOUNTAIN_TRAIL_SUMMIT_EXIT },
    { Rando::EntranceType::WarpSong, RR_BOLERO_OF_FIRE_WARP, RR_DMC_PAD_ENTRY, ENTR_DEATH_MOUNTAIN_CRATER_WARP_PAD,
      RR_NONE, RR_NONE, 0 },
    { Rando::EntranceType::BlueWarp, RR_FIRE_TEMPLE_BOSS_ROOM, RR_DMC_PAD_ENTRY,
      ENTR_DEATH_MOUNTAIN_CRATER_FIRE_TEMPLE_BLUE_WARP, RR_NONE, RR_NONE, 0 },
};

void DmcExpectEntranceRow(const DmcEntranceRow& row) {
    const std::string name = RegionTable(row.parent)->regionName + " -> " + RegionTable(row.connected)->regionName;
    const Rando::Entrance* exit = DmcFindExit(row.parent, row.connected);
    if (exit == nullptr) {
        GateFail("DE %s: the region graph has no such exit", name.c_str());
        return;
    }
    bool ok = true;
    if (exit->GetIndex() != row.index || exit->GetType() != row.type) {
        GateFail("DE %s: the table stamped index 0x%04X type %d, upstream's row is 0x%04X type %d", name.c_str(),
                 (unsigned)(uint16_t)exit->GetIndex(), (int)exit->GetType(), (unsigned)(uint16_t)row.index,
                 (int)row.type);
        ok = false;
    }
    const Rando::Entrance* reverse = exit->GetReverse();
    if (row.reverseParent == RR_NONE) {
        if (reverse != nullptr) {
            GateFail("DE %s: a one-way row came back bound to a reverse", name.c_str());
            ok = false;
        }
    } else if (reverse == nullptr || reverse->GetParentRegionKey() != row.reverseParent ||
               reverse->GetConnectedRegionKey() != row.reverseConnected || reverse->GetIndex() != row.reverseIndex ||
               reverse->GetType() != row.type) {
        GateFail("DE %s: the reverse is %d -> %d index 0x%04X, upstream's is %d -> %d index 0x%04X", name.c_str(),
                 reverse != nullptr ? (int)reverse->GetParentRegionKey() : -1,
                 reverse != nullptr ? (int)reverse->GetConnectedRegionKey() : -1,
                 reverse != nullptr ? (unsigned)(uint16_t)reverse->GetIndex() : 0u, (int)row.reverseParent,
                 (int)row.reverseConnected, (unsigned)(uint16_t)row.reverseIndex);
        ok = false;
    }
    if (ok) {
        printf("[TEST]   ok: DE %s: index 0x%04X%s\n", name.c_str(), (unsigned)(uint16_t)row.index,
               row.reverseParent == RR_NONE ? ", one way" : ", reverse bound");
    }
}

void RunDmcRefactorLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    uint8_t savedTricks[sizeof(kDmcForcedTricks) / sizeof(kDmcForcedTricks[0])];
    for (size_t i = 0; i < sizeof(kDmcForcedTricks) / sizeof(kDmcForcedTricks[0]); i++) {
        savedTricks[i] = randoCtx->GetTrickOption(kDmcForcedTricks[i]).Get();
        randoCtx->GetTrickOption(kDmcForcedTricks[i]).Set(0);
    }
    const uint8_t savedHearts = randoCtx->GetOption(RSK_STARTING_HEARTS).Get();
    const uint8_t savedDamage = randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Get();
    const uint8_t savedShuffleClimb = randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Get();
    const uint8_t savedDungeonEr = randoCtx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Get();
    const uint8_t savedBunnyHood = randoCtx->GetOption(RSK_BUNNY_HOOD).Get();
    const uint8_t savedMasksAsAdult = randoCtx->GetOption(RSK_MASKS_AS_ADULT).Get();
    const uint8_t savedMaskQuest = randoCtx->GetOption(RSK_MASK_QUEST).Get();
    const uint8_t savedSkipBeans = randoCtx->GetOption(RSK_SKIP_PLANTING_BEANS).Get();
    const uint8_t savedStartingBeans = randoCtx->GetOption(RSK_STARTING_BEANS).Get();
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(RO_DAMAGE_MULTIPLIER_DEFAULT);
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(0);
    randoCtx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Set(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(RO_MASK_QUEST_SHUFFLE);
    randoCtx->GetOption(RSK_SKIP_PLANTING_BEANS).Set(RO_GENERIC_OFF);
    randoCtx->GetOption(RSK_STARTING_BEANS).Set(RO_GENERIC_OFF);

    // ---- DX: every region the refactor adds is built ---------------------------
    int unbuilt = 0;
    for (int r = RR_DMC_UPPER_ENTRY; r <= RR_DMC_SCRUB_GROTTO; r++) {
        if (RegionTable((RandomizerRegion)r)->regionName.empty()) {
            GateFail("DX region %d: RegionTable_Init built no crater region for it", r);
            unbuilt++;
        }
    }
    if (unbuilt == 0) {
        printf("[TEST]   ok: DX the crater's %d regions are built\n",
               (int)(RR_DMC_SCRUB_GROTTO - RR_DMC_UPPER_ENTRY + 1));
    }

    // ---- DT: each exit and each Piece of Heart at its heart threshold ------------
    DmcResetHearts(randoCtx, 3);
    if (!logic->HasItem(RG_CLIMB) || logic->Health() != 3 * 16 || logic->FireTimer() != 0) {
        GateFail("DT: the reset holds Climb %d, health %d, heat timer %d (expected Climb, 48, 0), so the "
                 "thresholds are not what the legs measure",
                 logic->HasItem(RG_CLIMB) ? 1 : 0, (int)logic->Health(), (int)logic->FireTimer());
    }
    for (const DmcExitThreshold& row : kDmcExitThresholds) {
        DmcExpectExitAt(randoCtx, row);
    }
    for (const DmcCheckThreshold& row : kDmcWallThresholds) {
        DmcExpectCheckAt(randoCtx, row);
    }
    randoCtx->GetTrickOption(RT_DMC_HOVER_BEAN_POH).Set(1);
    for (const DmcCheckThreshold& row : kDmcVolcanoThresholds) {
        DmcExpectCheckAt(randoCtx, row);
    }
    randoCtx->GetTrickOption(RT_DMC_HOVER_BEAN_POH).Set(0);
    DmcResetHoverAdult(randoCtx, 20, false);
    ExpectAt("DT volcano PoH from the upper entry, adult with Hover Boots, 20 hearts, RT_DMC_HOVER_BEAN_POH off",
             RR_DMC_UPPER_ENTRY, RC_DMC_VOLCANO_FREESTANDING_POH, true, false,
             "the hover to the volcano Piece of Heart is in logic without its trick");

    // ---- DF: the heat timer side of each threshold ---------------------------------
    DmcResetHoverAdult(randoCtx, 1, false);
    Give(RG_GORON_TUNIC);
    ExpectExit("DF1 DMC upper entry to the centre, adult with Hover Boots and the Goron Tunic, one heart",
               RR_DMC_UPPER_ENTRY, RR_DMC_CENTRAL, true, true, "the Goron Tunic does not lift the crater's heat");
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(1);
    DmcResetHoverAdult(randoCtx, 8, false);
    ExpectExit("DF2 DMC upper entry to the centre, adult with Hover Boots, 8 hearts, Fewer Tunic Requirements",
               RR_DMC_UPPER_ENTRY, RR_DMC_CENTRAL, true, true,
               "8 hearts (a 64-unit heat timer) do not cross the crater under Fewer Tunic Requirements");
    DmcResetHoverAdult(randoCtx, 7, false);
    ExpectExit("DF3 DMC upper entry to the centre, adult with Hover Boots, 7 hearts, Fewer Tunic Requirements",
               RR_DMC_UPPER_ENTRY, RR_DMC_CENTRAL, true, false,
               "7 hearts (a 56-unit heat timer) cross the crater under Fewer Tunic Requirements");
    randoCtx->GetTrickOption(RT_FEWER_TUNIC_REQUIREMENTS).Set(0);

    // ---- DV: LOGIC_DMC_BOULDER ------------------------------------------------------
    DmcResetHearts(randoCtx, 3);
    FireEvents(RR_DMC_BLOCKED_EXIT, true);
    ExpectEvent("DV1 DMC blocked exit, adult with nothing", LOGIC_DMC_BOULDER, false,
                "the boulder breaks without the hammer");
    Give(RG_MEGATON_HAMMER);
    FireEvents(RR_DMC_BLOCKED_EXIT, true);
    ExpectEvent("DV2 DMC blocked exit, adult with the hammer", LOGIC_DMC_BOULDER, true,
                "the hammer does not break the boulder from the blocked exit (7522f31cc)");
    DmcResetHearts(randoCtx, 3);
    Give(RG_MEGATON_HAMMER);
    FireEvents(RR_DMC_SCRUB, true);
    ExpectEvent("DV3 DMC scrub ledge, adult with the hammer, RT_DMC_BOULDER_JS off", LOGIC_DMC_BOULDER, false,
                "the scrub ledge breaks the boulder without the jumpslash trick");
    randoCtx->GetTrickOption(RT_DMC_BOULDER_JS).Set(1);
    FireEvents(RR_DMC_SCRUB, true);
    ExpectEvent("DV4 DMC scrub ledge, adult with the hammer, RT_DMC_BOULDER_JS on", LOGIC_DMC_BOULDER, true,
                "the jumpslash trick does not break the boulder from the scrub ledge (7522f31cc)");
    randoCtx->GetTrickOption(RT_DMC_BOULDER_JS).Set(0);

    // ---- DU: DMCUpperToPots, through the upper entry's blocked-exit branch (5 hearts) ----
    DmcResetHearts(randoCtx, 5);
    ExpectExit("DU1 DMC upper entry to the blocked exit, adult with nothing, 5 hearts", RR_DMC_UPPER_ENTRY,
               RR_DMC_BLOCKED_EXIT, true, false, "the upper entry reaches the blocked exit past the boulder");
    logic->Set(LOGIC_DMC_BOULDER, true);
    ExpectExit("DU2 DMC upper entry to the blocked exit, adult, boulder broken, 5 hearts", RR_DMC_UPPER_ENTRY,
               RR_DMC_BLOCKED_EXIT, true, true, "a broken boulder does not open the way down (7522f31cc)");
    DmcResetHearts(randoCtx, 5);
    randoCtx->GetTrickOption(RT_DMC_BOULDER_SKIP).Set(1);
    ExpectExit("DU3 DMC upper entry to the blocked exit, adult, RT_DMC_BOULDER_SKIP, 5 hearts", RR_DMC_UPPER_ENTRY,
               RR_DMC_BLOCKED_EXIT, true, true, "the boulder skip does not open the way down (7522f31cc)");
    ExpectExit("DU4 DMC upper entry to the blocked exit, child, RT_DMC_BOULDER_SKIP, 5 hearts", RR_DMC_UPPER_ENTRY,
               RR_DMC_BLOCKED_EXIT, false, false, "the boulder skip is a child's");
    randoCtx->GetTrickOption(RT_DMC_BOULDER_SKIP).Set(0);
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(4);
    ResetLogicWithBunnyHood();
    BhlExitOffOn(randoCtx, "DU5 DMC upper entry to the blocked exit, child with the hood, 5 hearts, setting off",
                 "DU6 DMC upper entry to the blocked exit, child with the hood, 5 hearts, setting on",
                 RR_DMC_UPPER_ENTRY, RR_DMC_BLOCKED_EXIT);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);

    // ---- DP: DMCPotsToPad, through the blocked entry's centre branch (8 hearts) --------
    DmcResetHearts(randoCtx, 8);
    ExpectExit("DP1 DMC blocked entry to the centre, adult with nothing, 8 hearts", RR_DMC_BLOCKED_ENTRY,
               RR_DMC_CENTRAL, true, false, "the bridge's gap is crossed with nothing");
    GiveHookshot(false);
    ExpectExit("DP2 DMC blocked entry to the centre, adult with the Hookshot, 8 hearts", RR_DMC_BLOCKED_ENTRY,
               RR_DMC_CENTRAL, true, true, "the Hookshot does not cross the bridge's gap (7522f31cc)");
    DmcResetHearts(randoCtx, 8);
    Give(RG_HYLIAN_SHIELD);
    randoCtx->GetTrickOption(RT_DMC_BOLERO_JUMP).Set(1);
    ExpectExit("DP3 DMC blocked entry to the centre, adult, shield, RT_DMC_BOLERO_JUMP, no Power Bracelet, 8 hearts",
               RR_DMC_BLOCKED_ENTRY, RR_DMC_CENTRAL, true, false, "the pot jump is in logic without lifting the pot");
    GiveGrab();
    ExpectExit("DP4 DMC blocked entry to the centre, adult, shield, Power Bracelet, RT_DMC_BOLERO_JUMP, 8 hearts",
               RR_DMC_BLOCKED_ENTRY, RR_DMC_CENTRAL, true, true,
               "the pot jump with a shield and the Power Bracelet does not cross the gap (7522f31cc)");
    randoCtx->GetTrickOption(RT_DMC_BOLERO_JUMP).Set(0);
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(7);
    ResetLogicWithBunnyHood();
    BhlExitOffOn(randoCtx, "DP5 DMC blocked entry to the centre, child with the hood and Climb, 8 hearts, setting off",
                 "DP6 DMC blocked entry to the centre, child with the hood and Climb, 8 hearts, setting on",
                 RR_DMC_BLOCKED_ENTRY, RR_DMC_CENTRAL);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);

    // ---- DQ, DB: DMCPadToPots and the bean, through the pad entry's pots exit (3 hearts) ----
    DmcResetHearts(randoCtx, 3);
    ExpectExit("DQ1 DMC pad entry to the pots, adult with nothing, 3 hearts", RR_DMC_PAD_ENTRY, RR_DMC_POTS, true,
               false, "the pad reaches the pots with nothing");
    GiveHookshot(false);
    ExpectExit("DQ2 DMC pad entry to the pots, adult with the Hookshot, 3 hearts", RR_DMC_PAD_ENTRY, RR_DMC_POTS, true,
               true, "the Hookshot does not cross back from the pad (7522f31cc)");
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(2);
    MaaResetAdultWithHood();
    GiveClimb();
    MaaExitOffOn(randoCtx, "DQ3 DMC pad entry to the pots, adult with the hood and Climb, Masks as Adult off",
                 "DQ4 DMC pad entry to the pots, adult with the hood and Climb, Masks as Adult on", RR_DMC_PAD_ENTRY,
                 RR_DMC_POTS);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);
    DmcResetHearts(randoCtx, 3);
    Give(RG_DEATH_MOUNTAIN_CRATER_BEAN_SOUL);
    randoCtx->GetOption(RSK_SKIP_PLANTING_BEANS).Set(RO_GENERIC_ON);
    randoCtx->GetOption(RSK_STARTING_BEANS).Set(RO_GENERIC_ON);
    ExpectExit("DB1 DMC pad entry to the pots, adult, the bean planted, 3 hearts", RR_DMC_PAD_ENTRY, RR_DMC_POTS, true,
               true, "the planted bean does not carry an adult from the pad to the pots (7522f31cc)");
    ExpectExit("DB2 DMC pad entry to the pots, child, the bean planted, 3 hearts", RR_DMC_PAD_ENTRY, RR_DMC_POTS, false,
               false, "the bean's platform carries a child");
    randoCtx->GetOption(RSK_SKIP_PLANTING_BEANS).Set(RO_GENERIC_OFF);
    randoCtx->GetOption(RSK_STARTING_BEANS).Set(RO_GENERIC_OFF);

    // ---- DS: the child's bean sprout keeps upstream's form before 55b52a26a: a
    // child planting a fresh bean, alone in the crater, Skip Planting Beans off -----
    DmcResetHearts(randoCtx, 3);
    Give(RG_DEATH_MOUNTAIN_CRATER_BEAN_SOUL);
    GiveFairyOcarina();
    Give(RG_SONG_OF_STORMS);
    Give(RG_OCARINA_A_BUTTON);
    Give(RG_OCARINA_C_UP_BUTTON);
    Give(RG_OCARINA_C_DOWN_BUTTON);
    ExpectAt("DS1 DMC bean sprout fairy, child with Song of Storms and the soul, no bean", RR_DMC_CENTRAL,
             RC_DMC_BEAN_SPROUT_FAIRY_1, false, false, "the sprout's fairy is in logic without a bean to plant");
    Give(RG_MAGIC_BEAN);
    ExpectAt("DS2 DMC bean sprout fairy, child with Song of Storms, the soul and a bean, no adult", RR_DMC_CENTRAL,
             RC_DMC_BEAN_SPROUT_FAIRY_1, false, true,
             "a child who plants a fresh bean alone cannot reach the sprout's fairy (SoH b52c31f85 form)");

    // ---- DD: DMCUpperToPad, the scarecrow jump from the upper entry (9 hearts) -----------
    DmcResetHearts(randoCtx, 9);
    DmcGiveDistantScarecrow();
    ExpectExit("DD1 DMC upper entry to the temple exit, adult with Longshot and scarecrow, 9 hearts, trick off",
               RR_DMC_UPPER_ENTRY, RR_DMC_TEMPLE_EXIT, true, false,
               "the scarecrow jump to the pad is in logic without RT_UNINTUITIVE_JUMPS");
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(1);
    ExpectExit("DD2 DMC upper entry to the temple exit, adult with Longshot and scarecrow, 9 hearts, trick on",
               RR_DMC_UPPER_ENTRY, RR_DMC_TEMPLE_EXIT, true, true,
               "the scarecrow jump to the pad is out of logic with RT_UNINTUITIVE_JUMPS (ec27775e8)");
    DmcResetHearts(randoCtx, 8);
    DmcGiveDistantScarecrow();
    ExpectExit("DD3 DMC upper entry to the temple exit, adult with Longshot and scarecrow, 8 hearts, trick on",
               RR_DMC_UPPER_ENTRY, RR_DMC_TEMPLE_EXIT, true, false,
               "the scarecrow jump to the pad is in logic with 8 hearts");
    randoCtx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Set(0);

    // ---- DC: a child reaches the temple exit only under dungeon entrance shuffle ------
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(13);
    ResetLogicWithBunnyHood();
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_ON);
    ExpectExit("DC1 DMC upper entry to the temple exit, child with the hood, 14 hearts, dungeon shuffle off",
               RR_DMC_UPPER_ENTRY, RR_DMC_TEMPLE_EXIT, false, false,
               "a child walks into the Fire Temple with dungeon entrance shuffle off");
    randoCtx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Set(RO_DUNGEON_ENTRANCE_SHUFFLE_ON);
    ExpectExit("DC2 DMC upper entry to the temple exit, child with the hood, 14 hearts, dungeon shuffle on",
               RR_DMC_UPPER_ENTRY, RR_DMC_TEMPLE_EXIT, false, true,
               "a child cannot reach the temple door under dungeon entrance shuffle (7522f31cc)");
    randoCtx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Set(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(RO_GENERIC_OFF);

    // ---- DE: entrance.cpp's rows (SetAllEntrancesData stamps them on the live graph) ----
    // Every exit a row names is looked up first: SetAllEntrancesData asserts on a
    // missing one, so a renamed exit fails here by name instead of crashing.
    bool graphHoldsRows = true;
    for (const DmcEntranceRow& row : kDmcEntranceRows) {
        if (DmcFindExit(row.parent, row.connected) == nullptr ||
            (row.reverseParent != RR_NONE && DmcFindExit(row.reverseParent, row.reverseConnected) == nullptr)) {
            GateFail("DE %d -> %d: a region of the row has no exit to the region upstream's row names", (int)row.parent,
                     (int)row.connected);
            graphHoldsRows = false;
        }
    }
    if (graphHoldsRows) {
        Rando::SetAllEntrancesData();
        for (const DmcEntranceRow& row : kDmcEntranceRows) {
            DmcExpectEntranceRow(row);
        }
    }

    for (size_t i = 0; i < sizeof(kDmcForcedTricks) / sizeof(kDmcForcedTricks[0]); i++) {
        randoCtx->GetTrickOption(kDmcForcedTricks[i]).Set(savedTricks[i]);
    }
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(savedHearts);
    randoCtx->GetOption(RSK_DAMAGE_MULTIPLIER).Set(savedDamage);
    randoCtx->GetOption(RSK_SHUFFLE_CLIMB).Set(savedShuffleClimb);
    randoCtx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Set(savedDungeonEr);
    randoCtx->GetOption(RSK_BUNNY_HOOD).Set(savedBunnyHood);
    randoCtx->GetOption(RSK_MASKS_AS_ADULT).Set(savedMasksAsAdult);
    randoCtx->GetOption(RSK_MASK_QUEST).Set(savedMaskQuest);
    randoCtx->GetOption(RSK_SKIP_PLANTING_BEANS).Set(savedSkipBeans);
    randoCtx->GetOption(RSK_STARTING_BEANS).Set(savedStartingBeans);
}

// ---- #1404: LOGIC_TALON/MALON_RETURNED_FROM_CASTLE, the events A and B read --
// Each leg fires one region's own events (FireEvents) and reads the event; a
// "not set" leg holds everything its line asks but the one term it locks.
void RunReturnedFromCastleLegs(const std::shared_ptr<Rando::Context>& randoCtx) {
    const uint8_t savedWeirdEgg = randoCtx->GetOption(RSK_SHUFFLE_WEIRD_EGG).Get();
    const uint8_t savedSkipChildZelda = randoCtx->GetOption(RSK_SKIP_CHILD_ZELDA).Get();
    const uint8_t savedDamageBoost = randoCtx->GetTrickOption(RT_DAMAGE_BOOST_SIMPLE).Get();
    // The drain ledge's other way in (castle_grounds.cpp:106) must not be what
    // opens it; no leg grants explosives either.
    randoCtx->GetTrickOption(RT_DAMAGE_BOOST_SIMPLE).Set(0);

    // ---- HC: the castle moat wakes Talon (castle_grounds.cpp:96) ------------
    ResetLogic();
    GiveWeirdEgg();
    GiveNut();
    if (!logic->HasItem(RG_WEIRD_EGG)) {
        GateFail("HC1 setup: Give(RG_WEIRD_EGG) left HasItem(RG_WEIRD_EGG) false, so the egg legs are vacuous");
    }
    FireEvents(RR_HC_MOAT, false);
    ExpectEvent("HC1 castle moat, child with the Weird Egg and the nut", LOGIC_TALON_RETURNED_FROM_CASTLE, true,
                "a child who hatches the egg and talks to Talon does not wake him, so the ranch's Talon and Malon "
                "checks never enter logic");
    // ---- the castle gate sends Malon home after Talon (castle_grounds.cpp:28)
    FireEvents(RR_HC_GATE, false);
    ExpectEvent("HC2 castle gate after HC1, child with the nut", LOGIC_MALON_RETURNED_FROM_CASTLE, true,
                "Talon woken and the nut do not send Malon home, so Song from Malon never enters logic");

    ResetLogic();
    GiveNut();
    FireEvents(RR_HC_MOAT, false);
    ExpectEvent("HC3 castle moat, child with the nut, no Weird Egg", LOGIC_TALON_RETURNED_FROM_CASTLE, false,
                "Talon wakes with no egg to hatch a cucco");
    ResetLogic();
    GiveWeirdEgg();
    FireEvents(RR_HC_MOAT, false);
    ExpectEvent("HC4 castle moat, child with the Weird Egg, no nut", LOGIC_TALON_RETURNED_FROM_CASTLE, false,
                "Talon goes home for a child who cannot talk to him");
    ResetLogic();
    GiveWeirdEgg();
    GiveNut();
    FireEvents(RR_HC_MOAT, true);
    ExpectEvent("HC5 castle moat, adult with the Weird Egg and the nut", LOGIC_TALON_RETURNED_FROM_CASTLE, false,
                "an adult wakes Talon with the child-only Weird Egg (CanUse(RG_WEIRD_EGG) lost its age term)");

    ResetLogic();
    logic->Set(LOGIC_TALON_RETURNED_FROM_CASTLE, true);
    FireEvents(RR_HC_GATE, false);
    ExpectEvent("HC6 castle gate, child, Talon home, no nut", LOGIC_MALON_RETURNED_FROM_CASTLE, false,
                "Malon goes home for a child who cannot talk to her");
    ResetLogic();
    GiveWeirdEgg();
    GiveNut();
    FireEvents(RR_HC_GATE, false);
    ExpectEvent("HC7 castle gate, child with the egg and the nut, Talon not woken", LOGIC_MALON_RETURNED_FROM_CASTLE,
                false, "Malon goes home before Talon is woken");

    // ---- the moat's drain ledge needs Talon woken and the bracelet (:106) ---
    ResetLogic();
    GiveGrab();
    ExpectExit("HC8 castle moat to drain ledge, child with the bracelet, Talon not woken", RR_HC_MOAT,
               RR_HC_DRAIN_LEDGE, false, false,
               "the drain ledge opens while Talon still sleeps on the crates (damage-boost trick off)");
    ResetLogic();
    logic->Set(LOGIC_TALON_RETURNED_FROM_CASTLE, true);
    ExpectExit("HC9 castle moat to drain ledge, child, Talon woken, no bracelet", RR_HC_MOAT, RR_HC_DRAIN_LEDGE, false,
               false, "the drain ledge opens for a child who cannot push the milk crates");
    GiveGrab();
    ExpectExit("HC10 castle moat to drain ledge, child, Talon woken, bracelet", RR_HC_MOAT, RR_HC_DRAIN_LEDGE, false,
               true, "a child with Talon woken and the bracelet cannot reach the drain ledge on the way to Zelda");

    // ---- RT: the root's starting events (root.cpp:29 Malon, :30 Talon) -----
    struct RootCase {
        const char* name;
        uint8_t weirdEgg;
        uint8_t skipChildZelda;
        bool expected;
        const char* why;
    };
    const RootCase rootCases[] = {
        { "RT1 root, Skip Waking Talon, Skip Child Zelda off, no egg, no letter", RO_WEIRD_EGG_SKIP_TALON, 0, true,
          "Skip Waking Talon does not start the file with Talon and Malon home" },
        { "RT2 root, Weird Egg Vanilla, Skip Child Zelda off", RO_WEIRD_EGG_VANILLA, 0, false,
          "Talon and Malon start home on a file that must wake Talon" },
        { "RT3 root, Weird Egg Shuffled, Skip Child Zelda off", RO_WEIRD_EGG_SHUFFLED, 0, false,
          "Talon and Malon start home on a file with the egg shuffled" },
        { "RT4 root, Weird Egg Vanilla, legacy Skip Child Zelda on", RO_WEIRD_EGG_VANILLA, 1, true,
          "the legacy Skip Child Zelda key (whose creation wakes Talon) no longer starts Talon and Malon home" },
    };
    for (const RootCase& rootCase : rootCases) {
        char leg[128];
        randoCtx->GetOption(RSK_SHUFFLE_WEIRD_EGG).Set(rootCase.weirdEgg);
        randoCtx->GetOption(RSK_SKIP_CHILD_ZELDA).Set(rootCase.skipChildZelda);
        ResetLogic();
        if (logic->HasItem(RG_WEIRD_EGG) || logic->HasItem(RG_ZELDAS_LETTER)) {
            GateFail("%s setup: Logic::Reset granted the egg or the letter", rootCase.name);
        }
        FireEvents(RR_ROOT, false);
        snprintf(leg, sizeof(leg), "%s: Talon", rootCase.name);
        ExpectEvent(leg, LOGIC_TALON_RETURNED_FROM_CASTLE, rootCase.expected, rootCase.why);
        snprintf(leg, sizeof(leg), "%s: Malon", rootCase.name);
        ExpectEvent(leg, LOGIC_MALON_RETURNED_FROM_CASTLE, rootCase.expected, rootCase.why);
    }

    randoCtx->GetOption(RSK_SHUFFLE_WEIRD_EGG).Set(savedWeirdEgg);
    randoCtx->GetOption(RSK_SKIP_CHILD_ZELDA).Set(savedSkipChildZelda);
    randoCtx->GetTrickOption(RT_DAMAGE_BOOST_SIMPLE).Set(savedDamageBoost);
}

} // namespace

extern "C" int OoT_LogicGates_RunHeadless(void) {
    printf("[TEST] oot-logic-gates: Song from Malon, Talon's chickens, the Thieves' Hideout carpenters and the carpet "
           "merchant need what the game needs, and Talon and Malon come home from the castle the way the game sends "
           "them (#1168, #1404)\n");
    sFailures = 0;

    // The same fallback Rando_HeadlessSeedTest uses when the bring-up did not
    // create the randomizer context (3drando/menu.cpp).
    auto randoCtx = Rando::Context::GetInstance();
    if (!randoCtx) {
        randoCtx = Rando::Context::CreateInstance();
        randoCtx->InitStaticData();
        Rando::Settings::GetInstance()->AssignContext(randoCtx);
        Rando::StaticData::InitItemTable();
        Rando::StaticData::InitLocationTable();
    }

    // The options the gates are about, forced and restored. Glitchless, because
    // LocationAccess::GetConditionsMet answers true for everything otherwise.
    const uint8_t savedLogicRules = randoCtx->GetOption(RSK_LOGIC_RULES).Get();
    const uint8_t savedShuffleSpeak = randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Get();
    const uint8_t savedShuffleGrab = randoCtx->GetOption(RSK_SHUFFLE_GRAB).Get();
    const uint8_t savedShufflePots = randoCtx->GetOption(RSK_SHUFFLE_POTS).Get();
    const uint8_t savedStartingHearts = randoCtx->GetOption(RSK_STARTING_HEARTS).Get();
    randoCtx->GetOption(RSK_LOGIC_RULES).Set(RO_LOGIC_GLITCHLESS);
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(1);
    randoCtx->GetOption(RSK_SHUFFLE_GRAB).Set(1);
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(2);

    // The world graph, bound to this context's logic (location_access.cpp).
    RegionTable_Init();
    if (!logic) {
        GateFail("RegionTable_Init bound no logic object");
        return 1;
    }
    const bool savedChild = logic->IsChild;
    const bool savedAdult = logic->IsAdult;
    const bool savedDay = logic->AtDay;
    const bool savedNight = logic->AtNight;

    // ---- A: Song from Malon needs Malon home (#1404, SoH #6938) and the Hylian
    // jabber nut (SoH c906c71f7), lon_lon_ranch.cpp's RC_SONG_FROM_MALON row.
    // Reaches fires only the ranch's own events, so the castle event is set by
    // hand, after the reset (Logic::Reset clears every event).
    ResetLogic();
    GiveFairyOcarina();
    GiveNut();
    GiveZeldasLetter();
    Expect("A0 Song from Malon, ocarina, nut and Zelda's Letter, Malon not home", RR_LON_LON_RANCH, RC_SONG_FROM_MALON,
           false,
           "Malon teaches her song at the ranch while she is still at the castle: the letter alone opens it (#1404)");
    ResetLogic();
    logic->Set(LOGIC_MALON_RETURNED_FROM_CASTLE, true);
    GiveFairyOcarina();
    Expect("A1 Song from Malon, Malon home, ocarina, no jabber nut", RR_LON_LON_RANCH, RC_SONG_FROM_MALON, false,
           "Malon teaches her song to a child who cannot talk to her (#1168)");
    GiveNut();
    Expect("A2 Song from Malon, Malon home, ocarina and nut, no letter", RR_LON_LON_RANCH, RC_SONG_FROM_MALON, true,
           "a child with Malon home, the ocarina and the nut cannot get Malon's song");

    // ---- B: Talon's chickens need Talon home (#1404) and the Power Bracelet
    // (SoH 63597ec63), lon_lon_ranch.cpp's RC_LLR_TALONS_CHICKENS row --------
    ResetLogic();
    GiveChildWallet();
    GiveNut();
    GiveGrab();
    GiveZeldasLetter();
    Expect("B0 Talon's chickens, wallet, nut, bracelet and Zelda's Letter, Talon not home", RR_LLR_TALONS_HOUSE,
           RC_LLR_TALONS_CHICKENS, false,
           "the cucco game is in logic while Talon is still asleep at the castle: the letter alone opens it (#1404)");
    ResetLogic();
    logic->Set(LOGIC_TALON_RETURNED_FROM_CASTLE, true);
    GiveChildWallet();
    GiveNut();
    Expect("B1 Talon's chickens, Talon home, wallet and nut, no Power Bracelet", RR_LLR_TALONS_HOUSE,
           RC_LLR_TALONS_CHICKENS, false, "the cucco game is in logic for a child who cannot pick up a cucco");
    GiveGrab();
    Expect("B2 Talon's chickens, Talon home, wallet, nut and bracelet, no letter", RR_LLR_TALONS_HOUSE,
           RC_LLR_TALONS_CHICKENS, true,
           "a child with Talon home, wallet, nut and bracelet cannot play the cucco game");

    // ---- HC/RT: where Malon's and Talon's returns come from (#1404) ---------
    RunReturnedFromCastleLegs(randoCtx);

    // ---- C: each carpenter needs the cell's "could free" event (SoH 1413ceb33)
    for (const Carpenter& carpenter : kCarpenters) {
        char leg[96];
        ResetLogic();
        GiveSticks();
        snprintf(leg, sizeof(leg), "C1 %s, no jabber nut", carpenter.name);
        Expect(leg, carpenter.region, carpenter.check, false,
               "the carpenter's check is in logic for a fighter who cannot talk to the carpenter");
        GiveNut();
        snprintf(leg, sizeof(leg), "C2 %s, with the jabber nut", carpenter.name);
        Expect(leg, carpenter.region, carpenter.check, true,
               "a fighter with the nut does not reach the carpenter (the cell's event did not fire)");
    }

    // ---- D: the carpet merchant's parentheses (SoH 0702b11b9) --------------
    // GetCheckPrice reads the merchant's ItemLocation through OTRGlobals.
    if (OTRGlobals::Instance == nullptr || !OTRGlobals::Instance->gRandoContext) {
        GateFail("D: no OTRGlobals randomizer context, so the merchant's price cannot be read");
    } else {
        Rando::ItemLocation* merchant = randoCtx->GetItemLocation(RC_WASTELAND_BOMBCHU_SALESMAN);
        const uint16_t savedPrice = merchant->GetPrice();
        merchant->SetPrice(60);
        if (merchant->GetPrice() != 60) {
            GateFail("D: the merchant's price did not take (a custom price is pinned), so the price legs are vacuous");
        } else {
            ResetLogic();
            GiveSticks();
            GiveChildWallet(); // 99 rupees
            Expect("D1 carpet merchant, jumpslash and wallet, no jabber nut", RR_HAUNTED_WASTELAND,
                   RC_WASTELAND_BOMBCHU_SALESMAN, false,
                   "a jumpslash alone satisfies the merchant: the `||` swallows the nut and price terms");
            GiveNut();
            Expect("D2 carpet merchant, nut and a 60-rupee price in a 99 wallet", RR_HAUNTED_WASTELAND,
                   RC_WASTELAND_BOMBCHU_SALESMAN, true, "the merchant is out of logic with the nut and the rupees");
            merchant->SetPrice(200);
            Expect("D3 carpet merchant, nut but a 200-rupee price in a 99 wallet", RR_HAUNTED_WASTELAND,
                   RC_WASTELAND_BOMBCHU_SALESMAN, false,
                   "the merchant is in logic for a price the wallet cannot hold: the price term is bypassed");
        }
        merchant->SetPrice(savedPrice);
    }

    // ---- E: each carpenter's row carries its own cell's key (SoH 2bc50f605)
    for (const Carpenter& carpenter : kCarpenters) {
        ExpectCarpenterRow(carpenter);
    }

    // ---- F: a pot counts only items its scene allows (#1282, SoH 041295f05) --
    randoCtx->GetOption(RSK_SHUFFLE_POTS).Set(RO_SHUFFLE_POTS_ALL);
    for (const HousePot& pot : kHousePots) {
        char leg[96];
        ResetLogic();
        GiveChildPotBreakers();
        snprintf(leg, sizeof(leg), "F1 %s, sword/sticks/slingshot/boomerang/bombs, no grab", pot.name);
        Expect(leg, pot.region, pot.check, false,
               "a weapon the house disables (B, hookshot and C items off) counts for its pot (#1282)");
        GiveGrab();
        snprintf(leg, sizeof(leg), "F2 %s, with grab", pot.name);
        Expect(leg, pot.region, pot.check, true, "a child who can lift and throw cannot break the house pot");
    }
    for (RandomizerCheck rc : { RC_LLR_TALONS_HOUSE_POT_1, RC_LLR_TALONS_HOUSE_POT_2, RC_LLR_TALONS_HOUSE_POT_3 }) {
        const int n = (int)(rc - RC_LLR_TALONS_HOUSE_POT_1) + 1;
        char leg[96];
        ResetLogic();
        GiveSticks();
        Give(RG_PROGRESSIVE_SLINGSHOT);
        Give(RG_BOOMERANG);
        Give(RG_PROGRESSIVE_BOMB_BAG);
        snprintf(leg, sizeof(leg), "F3 Talon's house pot %d, sticks/slingshot/boomerang/bombs, no grab", n);
        Expect(leg, RR_LLR_TALONS_HOUSE, rc, false, "an item Talon's house disables counts for its pot (#1282)");
        Give(RG_KOKIRI_SWORD);
        snprintf(leg, sizeof(leg), "F4 Talon's house pot %d, plus the Kokiri Sword", n);
        Expect(leg, RR_LLR_TALONS_HOUSE, rc, true, "the sword (B is allowed in Talon's house) does not break the pot");
    }
    for (RandomizerCheck rc : { RC_MK_GUARD_HOUSE_ADULT_POT_1, RC_MK_GUARD_HOUSE_ADULT_POT_11 }) {
        const int n = (int)(rc - RC_MK_GUARD_HOUSE_ADULT_POT_1) + 1;
        char leg[96];
        ResetLogic();
        Give(RG_PROGRESSIVE_HOOKSHOT);
        Give(RG_PROGRESSIVE_HOOKSHOT); // the longshot
        snprintf(leg, sizeof(leg), "F5 guard house adult pot %d, longshot only, no grab", n);
        ExpectAs(true, leg, RR_MARKET_GUARD_HOUSE, rc, false,
                 "the hookshot/longshot the guard house disables counts for its pot (#1282)");
        Give(RG_PROGRESSIVE_BOMB_BAG);
        snprintf(leg, sizeof(leg), "F6 guard house adult pot %d, plus a bomb bag", n);
        ExpectAs(true, leg, RR_MARKET_GUARD_HOUSE, rc, true, "bombs (allowed in the guard house) do not break its pot");
    }
    ResetLogic();
    Expect("F7 Goron City pot, nothing", RR_GORON_CITY, RC_GC_LOWER_STAIRCASE_POT_1, false,
           "a Goron City pot is in logic with no way to break it");
    Give(RG_KOKIRI_SWORD);
    Expect("F8 Goron City pot, Kokiri Sword only", RR_GORON_CITY, RC_GC_LOWER_STAIRCASE_POT_1, true,
           "the sword no longer breaks a pot in a scene that allows it");

    // ---- F..P: the Shuffle Climb ladders (#1277, SoH bundle O5) -------------
    RunShuffleClimbLegs(randoCtx);
    // ---- Q..X: Hookshot Ladders and ladder cleanups (#1300, SoH bundle O28) -
    RunHookshotLaddersLegs(randoCtx);
    // ---- Y..Z: health in 16-per-heart units, the DMT rockfall (#1397) -------
    RunHealthLegs(randoCtx);
    // ---- DX..DE: the Death Mountain Crater refactor (#1647 O28, SoH 7522f31cc) --
    RunDmcRefactorLegs(randoCtx);
    // ---- VC: Voidout Collection (SoH e86e0ff69, #1647) -----------------------
    RunVoidoutCollectionLegs(randoCtx);
    // ---- LH, GV, GC: the SoH logic batch (156c2a4a, 28ab1206) ---------------
    RunSohLogicBatchLegs(randoCtx);
    // ---- FT: the Fire Trial split by door (139381a0e, 88099e6eb) -------------
    RunFireTrialSplitLegs(randoCtx);
    // ---- BH, LW: the Bunny Hood Effect seed setting (eca767dcb, e7f1801f0) -----
    RunBunnyHoodLegs(randoCtx);
    // ---- restore -------------------------------------------------------------
    randoCtx->GetOption(RSK_LOGIC_RULES).Set(savedLogicRules);
    randoCtx->GetOption(RSK_SHUFFLE_SPEAK).Set(savedShuffleSpeak);
    randoCtx->GetOption(RSK_SHUFFLE_GRAB).Set(savedShuffleGrab);
    randoCtx->GetOption(RSK_SHUFFLE_POTS).Set(savedShufflePots);
    randoCtx->GetOption(RSK_STARTING_HEARTS).Set(savedStartingHearts);
    logic->Reset(true);
    logic->IsChild = savedChild;
    logic->IsAdult = savedAdult;
    logic->AtDay = savedDay;
    logic->AtNight = savedNight;

    if (sFailures == 0) {
        printf("[TEST] PASS: oot-logic-gates\n");
    } else {
        printf("[TEST] FAIL: oot-logic-gates: %d leg(s) failed\n", sFailures);
    }
    return sFailures;
}
