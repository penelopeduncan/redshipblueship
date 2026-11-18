/**
 * @file BlastMaskOoT.cpp
 * @brief Majora's Mask's Blast Mask as an Ocarina of Time item (#979, epic #975
 *        P4; OoTMM blastMaskOot, sharedMaskBlast and blastMaskCooldown).
 *
 * THE ITEM. Under the frozen Blast Mask (OoT) rule (RSBS_XITEM_BLAST_MASK_OOT,
 * a Cross-Game Rules row, off by default) OoT's pool gains RG_BLAST_MASK
 * (3drando/item_pool.cpp). Its get-item sets RAND_INF_OBTAINED_BLAST_MASK and
 * fills an empty child trade slot with the custom ItemID ITEM_MASK_BLAST, as the
 * eight masks' get-item does (randomizer.cpp). ITEM_MASK_BLAST comes after
 * ITEM_ROCS_FEATHER and, like it, is never SLOT()-indexed: OoT_gItemSlots has 56
 * entries and every give and read here goes through ITEM_TRADE_CHILD's slot.
 * Its icon is Majora's Mask's own (OoT_gItemIcons' custom row, curated), and the
 * pause menu names it through VB_DRAW_CUSTOM_ITEM_NAME from MM's own name
 * texture once the curated half carries it.
 *
 * WEARING IT. The P3 groundwork (#977, ForeignMaskOoT.cpp) owns the worn field
 * and the draw; this file adds what an item needs:
 *  - OWNERSHIP: OoT's source (Combo_ForeignMask_SetOwnedSource) answers the
 *    RAND_INF flag, and Combo_ForeignMask_Owned adds the frozen rule, so a world
 *    that did not add the mask can never own it.
 *  - THE CHILD TRADE CYCLE: the ItemID joins it (OoT_ChildTradeCycle_RegisterExtra)
 *    while owned under the rule, after the Mask of Truth.
 *  - THE C BUTTON: VB_CHANGE_HELD_ITEM_AND_USE_ITEM puts it on or takes it off
 *    (OoT_ForeignMask_Use), as OoTMM's Player_UseItem toggles its masks. The
 *    custom ItemID never reaches OoT_Player_UseItem (sItemActions names it
 *    PLAYER_IA_NONE).
 *  - OOT'S BUTTON RULE: a worn mask comes off when no C button (or D-pad slot)
 *    holds it, unless SoH's Persistent Masks is on (OoT_Player_ProcessItemButtons'
 *    rule for a native mask).
 *
 * THE BLAST. While it is worn, B is the mask's (OoTMM's
 * Player_ProcessItemButtonsWrapper, #1718): z_player.c offers the B press to
 * OoT_BlastMask_OnItemButtons from OoT_Player_UpdateItems just before
 * OoT_Player_ProcessItemButtons, the call OoTMM patches with its wrapper, so the
 * press is the mask's only on a frame on which the game processes the item
 * buttons. There it is taken out of Link's input before the item buttons and his
 * action handlers read it, so the sword does not swing, and fires a blast at
 * Link's world.pos when the cooldown is spent: OoT's own bomb (ACTOR_EN_BOM,
 * BOMB_BODY) with its timer at 0 and its explosion sphere centered at spawn (as
 * MM_EnBom_Init seeds collider2, #1615), which explodes on its first update.
 *
 * THE COOLDOWN (OoTMM blastMaskCooldown): the frozen record's byte, in OoTMM's
 * frames (Combo_BlastMaskCooldownFrames; Default is MM's own 310), one blast per
 * cooldown (Combo_BlastMask_Step). It counts down once a frame in the player's
 * ShouldActorUpdate, before his update, as OoTMM's Player_UpdateWrapper counts
 * it down before Player_Update, so a frame without the item buttons still
 * spends it. Majora's Mask's own No Blast Mask Cooldown
 * enhancement composes over it: on, there is no cooldown, here as in Termina.
 * While it runs, the worn mask shows MM's glow pass (ForeignMaskOoT.cpp).
 *
 * ADULT LINK wears and blasts with it exactly as the child does while
 * OoT_MasksEquippableAsAdult() (operator 2026-11-15, "Make them adult-wearable";
 * ForeignMaskOoT.cpp's AgeMayWear). An armed mask takes B from every adult sword.
 *
 * THE LOGIC is logic.cpp's (CanUse(RG_BLAST_MASK): a child, or an adult under
 * the seed's Masks as Adult, under the frozen rule; OoTMM's can_use_mask_blast),
 * and the shared extension's pair is the bag's (both games register their copy
 * at RSBS_XITEM_SHARED_BLAST, GameExports_SingleExe.cpp).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstring>
#include <string>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/draw.h"
#include "ForeignMaskOoT.h"
#include "BlastMaskOoT.h"

#include "context.h"          // src/common: Context_GetCurrentGame
#include "cvar_shared_keys.h" // src/common: RSBS_CVAR_MM_MASKS_NO_BLAST_MASK_COOLDOWN
#include "foreign_items.h"    // src/common: the rule bits, the frozen accessors, Combo_GetForeignItemByNameFor
#include "foreign_masks.h"    // src/common: the worn-mask table, ownership, the cooldown table

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Bom/z_en_bom.h"
extern PlayState* OoT_gPlayState;
// ForeignModelHostOoT.cpp: an MM item's model, drawn by OoT as the host.
int OoT_ForeignModel_DrawForeignItem(PlayState* play, uint8_t originGame, uint16_t id);
}

namespace {

// Majora's Mask's own pause name texture, in the curated half
// (assets/crossgame/manifest.txt). Copied into the pause context by path.
const char kBlastMaskNameTex[] = "__OTR__item_name_static/gItemNameBlastMaskENGTex";

// The frames left before the next blast (0: ready).
uint16_t sCooldownLeft = 0;

bool PathMounted(const char* path) {
    static const char kOtr[] = "__OTR__";
    if (path == nullptr || std::strncmp(path, kOtr, sizeof(kOtr) - 1) != 0) {
        return false;
    }
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return false;
    }
    auto archives = ctx->GetResourceManager()->GetArchiveManager();
    return archives != nullptr && archives->HasFile(std::string(path + sizeof(kOtr) - 1));
}

/** OoT's ownership of its foreign masks: each one's RAND_INF flag (the Blast
 *  Mask's, and the Stone Mask's, StoneMaskOoT.cpp). Kamaro (the rest of P4) adds
 *  its case here. */
int ForeignMasksOwned(uint8_t id) {
    switch (id) {
        case COMBO_FOREIGN_MASK_MM_BLAST:
            return Flags_GetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK) ? 1 : 0;
        case COMBO_FOREIGN_MASK_MM_STONE:
            return Flags_GetRandomizerInf(RAND_INF_OBTAINED_STONE_MASK) ? 1 : 0;
        default:
            return 0;
    }
}

/** The child trade cycle's question: owned AND added by the frozen world. */
int BlastMaskInTradeCycle(void) {
    return Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST);
}

/** Is ITEM_MASK_BLAST equipped on a C button (or, with SoH's D-pad equips, a
 *  D-pad slot)? The equips themselves, not C_BTN_ITEM / DPAD_ITEM, which read
 *  ITEM_NONE while a scene or a textbox disables the buttons: a mask stays on
 *  through that, as a native one does (ProcessItemButtons does not run then). */
bool BlastMaskOnButtons() {
    for (int i = 1; i <= 3; i++) {
        if (gSaveContext.equips.buttonItems[i] == ITEM_MASK_BLAST) {
            return true;
        }
    }
    if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) != 0) {
        for (int i = 4; i <= 7; i++) {
            if (gSaveContext.equips.buttonItems[i] == ITEM_MASK_BLAST) {
                return true;
            }
        }
    }
    return false;
}

/** May a B press be the mask's? Asked only at the item buttons
 *  (OoT_BlastMask_OnItemButtons, called from OoT_Player_UpdateItems just before
 *  OoT_Player_ProcessItemButtons, where OoTMM patches in its
 *  Player_ProcessItemButtonsWrapper), so only on a frame on which the game
 *  processes the item buttons: Link's action reached OoT_Player_UpdateUpperBody
 *  (the only caller of OoT_Player_UpdateItems), and OoT_Player_UpdateItems' own
 *  gate (the held item settled, health, no cutscene or csAction, no shooting
 *  gallery, the main camera, no transition starting, the timer not stopped)
 *  passed. The actions that never reach the item buttons never ask, on land as
 *  in water (#1714, #1718): among them a sword swing, a jump attack, the roll
 *  (but for the end of the recovery after a bonk), being knocked down and lying
 *  there, the electric shock, being frozen, surfacing for breath, being hit in
 *  water, arriving in a scene swimming and dying in water. So in water this gate
 *  has no test of its own: the item buttons run while Link treads water or swims
 *  at the surface, sinks, dives, or walks the bottom on the Iron Boots, and so
 *  does the blast (#1701, #1714, operator 2026-11-17, "yes follow ootmm"). On top
 *  of that it keeps OoTMM's wrapper's own tests (not carrying an actor, not in a
 *  cutscene, the hookshot not held unfired, which is func_8008F128 and OoTMM's
 *  Player_UsingItem, not on Epona) and this build's other refusals, some of
 *  which OoTMM does not make: input disabled, talking, a dead Link, getting an
 *  item, first person, an item cutscene, hanging from or climbing a ledge, a
 *  ladder or vine, a crawlspace and hookshot flight (#1647). The health,
 *  cutscene, shooting gallery, camera and transition tests repeat
 *  OoT_Player_UpdateItems' gate. */
bool PlayerFreeToBlast(PlayState* play, Player* player) {
    const uint32_t busy = PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                          PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FIRST_PERSON |
                          PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE |
                          PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                          PLAYER_STATE1_CLIMBING_LADDER;
    if (func_8008F128(player) || (player->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT) != 0) {
        return false;
    }
    return (player->stateFlags1 & busy) == 0u && (player->stateFlags2 & PLAYER_STATE2_CRAWLING) == 0u &&
           player->rideActor == nullptr && gSaveContext.health != 0 && play->csCtx.state == CS_STATE_IDLE &&
           player->csAction == 0 && play->shootingGalleryStatus == 0 && play->activeCamera == MAIN_CAM &&
           play->transitionTrigger != TRANS_TRIGGER_START;
}

/** One blast at `s` (Link's world.pos): OoT's own bomb with its fuse spent and
 *  its explosion sphere centered at spawn (MM_EnBom_Init parity, #1615). */
void SpawnBlast(PlayState* play, const ComboBlastMaskSpawn& s) {
    ActorContext* ctx = &play->actorCtx;
    EnBom* bomb = (EnBom*)OoT_Actor_Spawn(ctx, play, ACTOR_EN_BOM, s.x, s.y, s.z, 0, 0, 0, BOMB_BODY, true);
    if (bomb != nullptr) {
        bomb->timer = 0;
        bomb->explosionCollider.elements[0].dim.worldSphere.center.x = s.centerX;
        bomb->explosionCollider.elements[0].dim.worldSphere.center.y = s.centerY;
        bomb->explosionCollider.elements[0].dim.worldSphere.center.z = s.centerZ;
    }
}

/** Link's ShouldActorUpdate, once a frame before his update: the worn mask's
 *  rules, then the cooldown's countdown (OoTMM's Player_UpdateWrapper counts it
 *  down before Player_Update). The B press is taken at the item buttons
 *  (OoT_BlastMask_OnItemButtons), not here. */
void BlastMaskBeforePlayerUpdate(void* actorRef, bool* should) {
    (void)should;
    if (Context_GetCurrentGame() != GAME_OOT || OoT_gPlayState == nullptr || actorRef == nullptr) {
        return;
    }
    PlayState* play = OoT_gPlayState;
    Player* player = static_cast<Player*>(actorRef);
    if (player != GET_PLAYER(play)) {
        return;
    }
    if (gSaveContext.linkAge != LINK_AGE_CHILD) {
        // #1647: an adult whose permission was withdrawn (a file without the
        // setting, the enhancement turned off) loses the mask before it can arm,
        // not at the end of this update (ForeignMaskOnPlayerUpdate).
        OoT_ForeignMask_ApplyRules(player);
    }
    if (OoT_ForeignMask_Worn() == COMBO_FOREIGN_MASK_MM_BLAST &&
        CVarGetInteger(CVAR_ENHANCEMENT("PersistentMasks"), 0) == 0 && !BlastMaskOnButtons()) {
        OoT_ForeignMask_Use(player, COMBO_FOREIGN_MASK_MM_BLAST); // the toggle takes the worn mask off
    }
    Combo_BlastMask_Step(&sCooldownLeft, 0, 0); // counts down only; fires nothing
}

/** The C-button press (VB_CHANGE_HELD_ITEM_AND_USE_ITEM): true when `item` is the
 *  Blast Mask, which never reaches OoT_Player_UseItem. */
bool BlastMaskUseItem(int32_t item) {
    if (item != (int32_t)ITEM_MASK_BLAST) {
        return false;
    }
    if (Context_GetCurrentGame() != GAME_OOT || OoT_gPlayState == nullptr) {
        return true;
    }
    Player* player = GET_PLAYER(OoT_gPlayState);
    if (Combo_ForeignMask_Owned(GAME_OOT, COMBO_FOREIGN_MASK_MM_BLAST) &&
        OoT_ForeignMask_Use(player, COMBO_FOREIGN_MASK_MM_BLAST) != 0) {
        OoT_Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
    }
    return true;
}

/** The pause menu's name (VB_DRAW_CUSTOM_ITEM_NAME): MM's own texture, when the
 *  curated half carries it; otherwise OoT's table names it as before. */
bool BlastMaskDrawName(uint32_t namedItem) {
    if (namedItem != (uint32_t)ITEM_MASK_BLAST || OoT_gPlayState == nullptr || !PathMounted(kBlastMaskNameTex)) {
        return false;
    }
    std::memcpy(OoT_gPlayState->pauseCtx.nameSegment, kBlastMaskNameTex, sizeof(kBlastMaskNameTex));
    return true;
}

void BlastMaskOnLoadGame(int32_t fileNum) {
    (void)fileNum;
    sCooldownLeft = 0;
    OoT_BlastMask_RegisterRuntime();
}

} // namespace

extern "C" bool OoT_BlastMaskOoTOn(void) {
    return Combo_ComboCrossItemRule(RSBS_XITEM_BLAST_MASK_OOT);
}

extern "C" int OoT_BlastMask_OnItemButtons(PlayState* play, Player* player, int bPressed) {
    if (bPressed == 0 || play == nullptr || player == nullptr || Context_GetCurrentGame() != GAME_OOT ||
        player != GET_PLAYER(play)) {
        return 0;
    }
    if (OoT_ForeignMask_Worn() != COMBO_FOREIGN_MASK_MM_BLAST || !PlayerFreeToBlast(play, player)) {
        return 0;
    }
    // OoTMM's Player_BlastMask: the press is the mask's even while the cooldown
    // runs, and blasts only once it is spent. The frame hook already counted this
    // frame down, so the step below fires without counting it twice.
    if (sCooldownLeft == 0) {
        const uint16_t frames = Combo_BlastMaskCooldownFor(
            Combo_ComboBlastMaskCooldown(), CVarGetInteger(RSBS_CVAR_MM_MASKS_NO_BLAST_MASK_COOLDOWN, 0) != 0 ? 1 : 0);
        const Vec3f& pos = player->actor.world.pos;
        ComboBlastMaskSpawn spawn;
        if (Combo_BlastMask_StepOotSpawn(&sCooldownLeft, 1, frames, pos.x, pos.y, pos.z, &spawn) != 0) {
            SpawnBlast(play, spawn);
        }
    }
    return 1;
}

extern "C" uint16_t OoT_BlastMask_CooldownLeft(void) {
    return sCooldownLeft;
}

extern "C" void OoT_BlastMask_SetCooldownForTest(uint16_t frames) {
    sCooldownLeft = frames;
}

extern "C" void OoT_BlastMask_RegisterRuntime(void) {
    Combo_ForeignMask_SetOwnedSource(GAME_OOT, ForeignMasksOwned);
    OoT_ChildTradeCycle_RegisterExtra((uint8_t)ITEM_MASK_BLAST, BlastMaskInTradeCycle); // 0 once registered
    // AGE_REQ_CHILD (z_kaleido_scope.h): a child trade mask, as OoT's own are
    // without Adult Masks; z_play.c and the pause screen free it for an adult
    // through OoT_ForeignMask_ApplyAgeReqs while OoT_MasksEquippableAsAdult()
    // (#1647). Reads no CVar. The 256-entry table covers the custom ItemID (#983).
    gItemAgeReqs[ITEM_MASK_BLAST] = LINK_AGE_CHILD;
}

/**
 * RG_BLAST_MASK's get-item model: Majora's Mask's own Blast Mask, drawn by OoT as
 * the host from the curated half (OoT_ForeignModel_DrawForeignItem); the Keaton
 * Mask's row when no archive carries it.
 */
extern "C" void Randomizer_DrawBlastMask(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    static SharedItem sMmBlast;
    static int sResolved = -1;
    if (play == nullptr) {
        return;
    }
    if (sResolved < 0) {
        sResolved = Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Blast Mask", &sMmBlast) ? 1 : 0;
    }
    if (sResolved == 1 && OoT_ForeignModel_DrawForeignItem(play, sMmBlast.originGame, sMmBlast.id) == 1) {
        return;
    }
    OoT_GetItem_Draw(play, GID_MASK_KEATON);
}

// Always registered: every hook acts only on the worn field, the custom ItemID or
// the rule, which a world without the Blast Mask (OoT) rule never holds.
extern "C" void OoT_BlastMask_RegisterHooks(void) {
    OoT_BlastMask_RegisterRuntime();
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_PLAYER, true, BlastMaskBeforePlayerUpdate);
    COND_HOOK(OnLoadGame, true, BlastMaskOnLoadGame);
    COND_VB_SHOULD(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, true, {
        const int32_t usedItem = va_arg(args, int32_t);
        if (BlastMaskUseItem(usedItem)) {
            *should = false;
        }
    });
    COND_VB_SHOULD(VB_DRAW_CUSTOM_ITEM_NAME, true, {
        const uint32_t namedItem = va_arg(args, uint32_t);
        if (BlastMaskDrawName(namedItem)) {
            *should = true;
        }
    });
}

static void RegisterBlastMaskOoT() {
    OoT_BlastMask_RegisterHooks();
}

static RegisterShipInitFunc initFunc(RegisterBlastMaskOoT);

#endif // RSBS_SINGLE_EXECUTABLE
