/**
 * @file oot_blast_mask_test.cpp
 * OoT's half of the Blast Mask locks (#979, epic #975 P4; the rules are
 * RSBS_XITEM_BLAST_MASK_OOT and RSBS_XITEM_SHARED_BLAST in
 * src/common/foreign_items.h, the item is
 * soh/Enhancements/randomizer/BlastMaskOoT.cpp).
 *
 * THE HELPERS the redship rows `blast-mask-oot-items` and
 * `blast-mask-oot-shrub-guardhouse` (src/common/tests/test_blast_mask_oot.c)
 * drive OoT's tables, live save, REAL logic, and headless PlayState actors
 * (`ACTOR_EN_BOM`, `ACTOR_EN_KUSA`, `ACTOR_OBJ_TSUBO`) through (#979, #1615).
 * Every decision is the production code's. They live here because only an OoT
 * TU has OoT's ItemIDs, its tables, `gSaveContext`, Rando::Logic, and actor
 * structs in scope.
 */

#include <cstdint>
#include <cstring>
#include <memory>

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/BlastMaskOoT.h"

extern "C" s32 Flags_GetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_SetRandomizerInf(RandomizerInf flag);
extern "C" void Flags_UnsetRandomizerInf(RandomizerInf flag);
// z_inventory.c: OoT_gItemIcons' entry count, which only its defining TU sees.
extern "C" const u32 OoT_gItemIconsCount;

/** The Blast Mask's custom ItemID, and Roc's Feather's (the first custom one). */
extern "C" uint8_t OoT_BlastMaskTest_ItemId(void) {
    return (uint8_t)ITEM_MASK_BLAST;
}

extern "C" uint8_t OoT_BlastMaskTest_RocsFeatherId(void) {
    return (uint8_t)ITEM_ROCS_FEATHER;
}

/** OoT_gItemSlots' entry count: an ItemID at or past it has no inventory slot,
 *  and SLOT() must never be asked for it. */
extern "C" int OoT_BlastMaskTest_ItemSlotsCount(void) {
    return (int)ARRAY_COUNT(OoT_gItemSlots);
}

/** OoT_gItemIcons' entry count, from its definition. */
extern "C" uint32_t OoT_BlastMaskTest_ItemIconsCount(void) {
    return (uint32_t)OoT_gItemIconsCount;
}

/** OoT_gItemIcons' row for `item` (a path), or NULL past the table. */
extern "C" const char* OoT_BlastMaskTest_Icon(uint8_t item) {
    return (uint32_t)item < (uint32_t)OoT_gItemIconsCount ? static_cast<const char*>(OoT_gItemIcons[item]) : nullptr;
}

/** gItemAgeReqs[item] (256 entries since #983), and the child age it should hold. */
extern "C" int OoT_BlastMaskTest_AgeReq(uint8_t item) {
    return (int)gItemAgeReqs[item];
}

extern "C" int OoT_BlastMaskTest_ChildAge(void) {
    return (int)LINK_AGE_CHILD;
}

/** OoT's pool id for the Blast Mask (RG_BLAST_MASK), and whether the item table
 *  names it an advancement item (what makes the bag class it PROGRESSION). */
extern "C" int OoT_BlastMaskTest_PoolId(void) {
    return (int)RG_BLAST_MASK;
}

extern "C" int OoT_BlastMaskTest_IsAdvancement(void) {
    return Rando::StaticData::RetrieveItem(RG_BLAST_MASK).IsAdvancement() ? 1 : 0;
}

/** Give (1) or take (0) OoT's ownership of the Blast Mask in the live save: its
 *  RAND_INF flag, which its get-item sets. */
extern "C" void OoT_BlastMaskTest_SetOwned(int held) {
    if (held != 0) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    } else {
        Flags_UnsetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    }
}

extern "C" int OoT_BlastMaskTest_Owned(void) {
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK) ? 1 : 0;
}

/** The live save's child trade slot, and a write to it. */
extern "C" int OoT_BlastMaskTest_TradeSlot(void) {
    return (int)INV_CONTENT(ITEM_TRADE_CHILD);
}

extern "C" void OoT_BlastMaskTest_SetTradeSlot(uint8_t item) {
    INV_CONTENT(ITEM_TRADE_CHILD) = item;
}

/**
 * OoT's REAL logic for the Blast Mask: a private Rando::Logic over a zeroed save
 * holding nothing but the mask's ownership flag (@p held), as a child or an
 * adult (@p child), under whatever cross-game item rules the caller froze.
 * @p which: 0 BlastOrSmash, 1 CanCutShrubs, 2 CanHitSwitch(ED_CLOSE),
 * 3 CanKillEnemy(RE_BEAMOS), 4 CanUse(RG_BLAST_MASK), else HasItem(RG_BLAST_MASK).
 * 1 or 0; -1 with no randomizer context.
 */
extern "C" int OoT_BlastMaskTest_Logic(int held, int child, int which) {
    std::shared_ptr<Rando::Context> ctx = Rando::Context::GetInstance();
    bool created = false;
    if (ctx == nullptr) {
        ctx = Rando::Context::CreateInstance();
        created = true;
    }
    if (ctx == nullptr) {
        return -1;
    }
    auto save = std::make_unique<SaveContext>();
    std::memset(save.get(), 0, sizeof(SaveContext));
    // Empty pockets: a zeroed slot holds ITEM_DEKU_STICK (0), which the logic's
    // inexact inventory checks (CheckInventory(item, false)) read as held.
    std::memset(save->inventory.items, ITEM_NONE, sizeof(save->inventory.items));
    auto probe = std::make_unique<Rando::Logic>();
    probe->SetContext(ctx);
    probe->SetSaveContext(save.get());
    probe->Reset(false);
    probe->IsChild = child != 0;
    probe->IsAdult = child == 0;
    probe->SetRandoInf((uint32_t)RAND_INF_OBTAINED_BLAST_MASK, held != 0);
    // #1647: Masks as Adult widens the adult's answer; this probe's adult is the setting off's,
    // whatever an earlier row left in the shared context (OoTLogicGates covers the setting on).
    const uint8_t savedMasksAsAdult = ctx->GetOption(RSK_MASKS_AS_ADULT).Get();
    ctx->GetOption(RSK_MASKS_AS_ADULT).Set(RO_GENERIC_OFF);
    bool result = false;
    switch (which) {
        case 0:
            result = probe->BlastOrSmash();
            break;
        case 1:
            result = probe->CanCutShrubs();
            break;
        case 2:
            result = probe->CanHitSwitch(ED_CLOSE);
            break;
        case 3:
            result = probe->CanKillEnemy(RE_BEAMOS);
            break;
        case 4:
            result = probe->CanUse(RG_BLAST_MASK);
            break;
        default:
            result = probe->HasItem(RG_BLAST_MASK);
            break;
    }
    ctx->GetOption(RSK_MASKS_AS_ADULT).Set(savedMasksAsAdult);
    probe.reset();
    if (created) {
        // Context and its Logic hold each other: let go of the one this row made.
        ctx->GetLogic()->SetContext(nullptr);
    }
    return result ? 1 : 0;
}

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <libultraship/bridge.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "context.h"
#include "cvar_shared_keys.h"
#include "foreign_items.h"
#include "foreign_masks.h"
#include "soh/ActorDB.h"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/gameconsole.h"
#include "soh/Enhancements/gameplaystats.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/nametag.h"
#include "soh/Enhancements/randomizer/ForeignMaskOoT.h"

extern "C" {
#include "functions.h"
#include "src/overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "src/overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "src/overlays/actors/ovl_Obj_Tsubo/z_obj_tsubo.h"
extern PlayState* OoT_gPlayState;
extern EffectSsInfo OoT_sEffectSsInfo;
void EnKusa_Main(EnKusa* thisx, PlayState* play);
void OoT_EnKusa_DoNothing(EnKusa* thisx, PlayState* play);
#include "overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h" // AGE_REQ_NONE
}

/** #1647: OoT_ForeignMask_ApplyAgeReqs, the trade slot's age row, and AGE_REQ_NONE, for the Items rows. */
extern "C" void OoT_BlastMaskTest_ApplyAgeReqs(int adultMayWear) {
    OoT_ForeignMask_ApplyAgeReqs(adultMayWear);
}

extern "C" int OoT_BlastMaskTest_AgeReqNone(void) {
    return (int)AGE_REQ_NONE;
}

extern "C" int OoT_BlastMaskTest_SlotAgeReq(void) {
    return (int)gSlotAgeReqs[SLOT_TRADE_CHILD];
}

extern "C" void OoT_BlastMaskTest_SetSlotAgeReq(int req) {
    gSlotAgeReqs[SLOT_TRADE_CHILD] = (u8)req;
}

template <typename H> struct Bm1615HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForID) forId;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForPtr) forPtr;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    Bm1615HooksAside() {
        Swap();
    }
    ~Bm1615HooksAside() {
        Swap();
    }
    void Swap() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        forId.swap(GameInteractor::RegisteredGameHooks<H>::functionsForID);
        forPtr.swap(GameInteractor::RegisteredGameHooks<H>::functionsForPtr);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

struct Bm1615SavedCVar {
    std::string name;
    bool existed = false;
    Ship::ConsoleVariableType type = Ship::ConsoleVariableType::Integer;
    int32_t integer = 0;
    float real = 0.0f;
    std::string text;
};

static PlayState sBm1615Play;
static Player sBm1615Player;
static GameInfo sBm1615GameInfo;
static Vec3s sBm1615FloorVtx[4];
static CollisionPoly sBm1615FloorPolys[2];
static SurfaceType sBm1615SurfaceTypes[1];
static CamData sBm1615CamData[1];
static CollisionHeader sBm1615FloorHeader;
static int sBm1615Failures = 0;

static void Bm1615Check(bool cond, const char* msg, int line) {
    if (!cond) {
        std::printf("[TEST] FAIL (blast-mask-oot-shrub-guardhouse): %s (%s:%d)\n", msg, __FILE__, line);
        sBm1615Failures++;
    }
}

static Bm1615SavedCVar Bm1615SaveCVar(const char* name) {
    Bm1615SavedCVar saved;
    saved.name = name;
    const std::shared_ptr<Ship::CVar> cvar = CVarGet(name);
    if (cvar == nullptr) {
        return saved;
    }
    saved.existed = true;
    saved.type = cvar->Type;
    if (cvar->Type == Ship::ConsoleVariableType::Integer) {
        saved.integer = cvar->Integer;
    } else if (cvar->Type == Ship::ConsoleVariableType::Float) {
        saved.real = cvar->Float;
    } else if (cvar->Type == Ship::ConsoleVariableType::String && cvar->String != nullptr) {
        saved.text = cvar->String;
    }
    return saved;
}

static void Bm1615RestoreCVar(const Bm1615SavedCVar& saved) {
    if (!saved.existed) {
        CVarClear(saved.name.c_str());
        return;
    }
    if (saved.type == Ship::ConsoleVariableType::Integer) {
        CVarSetInteger(saved.name.c_str(), saved.integer);
    } else if (saved.type == Ship::ConsoleVariableType::Float) {
        CVarSetFloat(saved.name.c_str(), saved.real);
    } else if (saved.type == Ship::ConsoleVariableType::String) {
        CVarSetString(saved.name.c_str(), saved.text.c_str());
    }
}

static void Bm1615NoopPlayerUpdate(Actor* thisx, PlayState* play) {
    (void)thisx;
    (void)play;
}

static void Bm1615InitFloorHeader(s16 floorY) {
    std::memset(sBm1615FloorVtx, 0, sizeof(sBm1615FloorVtx));
    sBm1615FloorVtx[0] = { -2000, floorY, -2000 };
    sBm1615FloorVtx[1] = { -2000, floorY, 2000 };
    sBm1615FloorVtx[2] = { 2000, floorY, 2000 };
    sBm1615FloorVtx[3] = { 2000, floorY, -2000 };

    std::memset(sBm1615FloorPolys, 0, sizeof(sBm1615FloorPolys));
    sBm1615FloorPolys[0].vtxData[0] = 0;
    sBm1615FloorPolys[0].vtxData[1] = 1;
    sBm1615FloorPolys[0].vtxData[2] = 2;
    sBm1615FloorPolys[0].normal = { 0, 0x7FFF, 0 };
    sBm1615FloorPolys[0].dist = (s16)(-floorY);
    sBm1615FloorPolys[1].vtxData[0] = 0;
    sBm1615FloorPolys[1].vtxData[1] = 2;
    sBm1615FloorPolys[1].vtxData[2] = 3;
    sBm1615FloorPolys[1].normal = { 0, 0x7FFF, 0 };
    sBm1615FloorPolys[1].dist = (s16)(-floorY);

    std::memset(sBm1615SurfaceTypes, 0, sizeof(sBm1615SurfaceTypes));
    std::memset(sBm1615CamData, 0, sizeof(sBm1615CamData));
    std::memset(&sBm1615FloorHeader, 0, sizeof(sBm1615FloorHeader));
    sBm1615FloorHeader.minBounds = { -2000, -400, -2000 };
    sBm1615FloorHeader.maxBounds = { 2000, 400, 2000 };
    sBm1615FloorHeader.numVertices = 4;
    sBm1615FloorHeader.vtxList = sBm1615FloorVtx;
    sBm1615FloorHeader.numPolygons = 2;
    sBm1615FloorHeader.polyList = sBm1615FloorPolys;
    sBm1615FloorHeader.surfaceTypeList = sBm1615SurfaceTypes;
    sBm1615FloorHeader.cameraDataList = sBm1615CamData;
    sBm1615FloorHeader.cameraDataListLen = 1;
}

static void Bm1615FreezeWorld(uint32_t flags, uint8_t cooldown) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x1615B1A5u;
    gComboCtx.sharedRandoSettingsHash = 0x16151615u;
    gComboCtx.mmProfileDigest = 0x0BADB1A5u;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    ComboItemRulesRecord rules;
    std::memset(&rules, 0, sizeof(rules));
    rules.crossItemFlags = flags;
    rules.blastMaskCooldown = cooldown;
    Combo_FreezeComboSettingsEx(&rec, &rules);
}

static void Bm1615ArmWearer(Player* player, s32 linkAge = LINK_AGE_CHILD) {
    std::memset(&gSaveContext, 0, sizeof(SaveContext));
    std::memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    std::memset(gSaveContext.equips.buttonItems, ITEM_NONE, sizeof(gSaveContext.equips.buttonItems));
    gSaveContext.fileNum = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = linkAge;
    gSaveContext.healthCapacity = 0x30;
    gSaveContext.health = 0x30;
    Flags_SetRandomizerInf(RAND_INF_OBTAINED_BLAST_MASK);
    INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_BLAST;
    gSaveContext.equips.buttonItems[1] = ITEM_MASK_BLAST;
    OoT_ForeignMask_SetStoredRaw(COMBO_FOREIGN_MASK_NONE);
    OoT_ForeignMask_Use(player, COMBO_FOREIGN_MASK_MM_BLAST);
    OoT_BlastMask_SetCooldownForTest(0);
}

static void Bm1615PlacePlayer(Player* player, f32 wx, f32 wy, f32 wz, f32 fx, f32 fy, f32 fz) {
    player->actor.home.pos = { wx, wy, wz };
    player->actor.world.pos = { wx, wy, wz };
    player->actor.prevPos = { wx, wy, wz };
    player->actor.focus.pos = { fx, fy, fz };
}

static void Bm1615BeginScene(s16 sceneNum, s16 floorY, std::vector<uint8_t>& thaBuf) {
    std::memset(&sBm1615Play, 0, sizeof(PlayState));
    std::memset(&sBm1615Player, 0, sizeof(Player));
    std::memset(&sBm1615GameInfo, 0, sizeof(GameInfo));

    Context_SetCurrentGame(GAME_OOT);
    gGameInfo = &sBm1615GameInfo;
    R_UPDATE_RATE = 3;
    OoT_gPlayState = &sBm1615Play;

    sBm1615Play.sceneNum = sceneNum;
    sBm1615Play.csCtx.state = CS_STATE_IDLE;
    sBm1615Play.cameraPtrs[MAIN_CAM] = &sBm1615Play.mainCamera;
    sBm1615Play.activeCamera = MAIN_CAM;
    sBm1615Play.transitionTrigger = TRANS_TRIGGER_OFF;
    sBm1615Play.objectCtx.num = 5;
    sBm1615Play.objectCtx.status[0].id = OBJECT_GAMEPLAY_KEEP;
    sBm1615Play.objectCtx.status[1].id = OBJECT_GAMEPLAY_FIELD_KEEP;
    sBm1615Play.objectCtx.status[2].id = OBJECT_KUSA;
    sBm1615Play.objectCtx.status[3].id = OBJECT_GAMEPLAY_DANGEON_KEEP;
    sBm1615Play.objectCtx.status[4].id = OBJECT_TSUBO;

    thaBuf.assign(512 * 1024, 0);
    OoT_GameAlloc_Init(&sBm1615Play.state.alloc);
    THA_Ct(&sBm1615Play.state.tha, thaBuf.data(), thaBuf.size());
    Bm1615InitFloorHeader(floorY);
    OoT_BgCheck_Allocate(&sBm1615Play.colCtx, &sBm1615Play, &sBm1615FloorHeader);
    OoT_EffectSs_InitInfo(&sBm1615Play, 0x20);
    OoT_CollisionCheck_InitContext(&sBm1615Play, &sBm1615Play.colChkCtx);
    Audio_ResetSounds();
    OoT_SoundSource_InitAll(&sBm1615Play);
    OoT_Quake_Init();

    sBm1615Player.actor.id = ACTOR_PLAYER;
    sBm1615Player.actor.category = ACTORCAT_PLAYER;
    sBm1615Player.actor.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    sBm1615Player.actor.update = Bm1615NoopPlayerUpdate;
    sBm1615Play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &sBm1615Player.actor;
    sBm1615Play.actorCtx.actorLists[ACTORCAT_PLAYER].length = 1;
}

static void Bm1615EndScene(PlayState* play) {
    for (int cat = 0; cat < (int)ARRAY_COUNT(play->actorCtx.actorLists); cat++) {
        if (cat == ACTORCAT_PLAYER) {
            continue;
        }
        Actor* actor = play->actorCtx.actorLists[cat].head;
        while (actor != nullptr) {
            actor = OoT_Actor_Delete(&play->actorCtx, actor, play);
        }
    }
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = nullptr;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].length = 0;
    OoT_EffectSs_ClearAll(play);
    THA_Dt(&play->state.tha);
    OoT_GameAlloc_Cleanup(&play->state.alloc);
}

/** One gameplay update + AT collision pass in OoT_Actor_UpdateAll's category
 *  order (ACTORCAT_PLAYER before ACTORCAT_EXPLOSIVE before ACTORCAT_PROP), with
 *  NO OoT_EnBom_Draw call: reproduces the exact instant-bomb lifecycle (#1615). */
static void Bm1615StepFrame(PlayState* play, Player* player, u16 pressButtons) {
    play->state.input[0].press.button = pressButtons;
    play->state.input[0].cur.button = pressButtons;
    OoT_CollisionCheck_ClearContext(play, &play->colChkCtx);

    for (int cat = 0; cat < (int)ARRAY_COUNT(play->actorCtx.actorLists); cat++) {
        Actor* actor = play->actorCtx.actorLists[cat].head;
        while (actor != nullptr) {
            if (actor->init != nullptr) {
                actor->init(actor, play);
                actor->init = nullptr;
                actor = actor->next;
                continue;
            }
            if (actor->update == nullptr) {
                if (cat != ACTORCAT_PLAYER) {
                    actor = OoT_Actor_Delete(&play->actorCtx, actor, play);
                } else {
                    actor = actor->next;
                }
                continue;
            }
            OoT_Math_Vec3f_Copy(&actor->prevPos, &actor->world.pos);
            actor->xzDistToPlayer = OoT_Actor_WorldDistXZToActor(actor, &player->actor);
            actor->yDistToPlayer = OoT_Actor_HeightDiff(actor, &player->actor);
            actor->xyzDistToPlayerSq = SQ(actor->xzDistToPlayer) + SQ(actor->yDistToPlayer);
            actor->yawTowardsPlayer = OoT_Actor_WorldYawTowardActor(actor, &player->actor);
            if (GameInteractor_ShouldActorUpdate(actor)) {
                actor->update(actor, play);
            }
            OoT_CollisionCheck_ResetDamage(&actor->colChkInfo);
            actor = actor->next;
        }
    }

    OoT_CollisionCheck_AT(play, &play->colChkCtx);
    play->gameplayFrames++;
    play->state.frames++;
}

static EnBom* Bm1615FindBomb(PlayState* play) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head;
    while (actor != nullptr) {
        if (actor->id == ACTOR_EN_BOM) {
            return (EnBom*)actor;
        }
        actor = actor->next;
    }
    return nullptr;
}

static bool Bm1615ActorInCat(PlayState* play, int cat, const Actor* target) {
    const Actor* actor = play->actorCtx.actorLists[cat].head;
    while (actor != nullptr) {
        if (actor == target) {
            return true;
        }
        actor = actor->next;
    }
    return false;
}

static EnKusa* Bm1615SpawnKusa(f32 x, f32 y, f32 z, s16 params) {
    return (EnKusa*)OoT_Actor_Spawn(&sBm1615Play.actorCtx, &sBm1615Play, ACTOR_EN_KUSA, x, y, z, 0, 0, 0, params, 0);
}

static ObjTsubo* Bm1615SpawnPot(f32 x, f32 y, f32 z) {
    ActorContext* ctx = &sBm1615Play.actorCtx;
    return (ObjTsubo*)OoT_Actor_Spawn(ctx, &sBm1615Play, ACTOR_OBJ_TSUBO, x, y, z, 0, 0, 0, 0x001F, 0);
}

static void Bm1615RunShrubLeg(std::vector<uint8_t>& thaBuf) {
    Bm1615FreezeWorld(RSBS_XITEM_BLAST_MASK_OOT, 0u);
    Bm1615BeginScene(SCENE_KOKIRI_FOREST, 140, thaBuf);
    Bm1615ArmWearer(&sBm1615Player);
    // Non-zero X/Y/Z (240, 140, -180). In play focus.pos follows Link's head on
    // every drawn frame (OoT_Player_PostLimbDrawGameplay); it is pinned far away
    // at (0, 184, 0) here only so a blast spawned at focus.pos instead of
    // world.pos is observable: it would miss the adjacent shrubs and cut the
    // (0, 140, 0) control shrub. A sphere left at (0, 0, 0) is caught by the
    // adjacent shrubs staying uncut (it is 140 units below the control).
    Bm1615PlacePlayer(&sBm1615Player, 240.0f, 140.0f, -180.0f, 0.0f, 184.0f, 0.0f);

    EnKusa* kusaNear0 = Bm1615SpawnKusa(264.0f, 140.0f, -180.0f, ENKUSA_TYPE_0);
    EnKusa* kusaNear2 = Bm1615SpawnKusa(240.0f, 140.0f, -156.0f, ENKUSA_TYPE_2);
    EnKusa* kusaOrigin = Bm1615SpawnKusa(0.0f, 140.0f, 0.0f, ENKUSA_TYPE_0);
    const bool spawnedAll = kusaNear0 != nullptr && kusaNear2 != nullptr && kusaOrigin != nullptr;
    Bm1615Check(spawnedAll, "EnKusa spawn failed", __LINE__);
    if (!spawnedAll) {
        Bm1615EndScene(&sBm1615Play);
        return;
    }
    const Actor* const kusaNear0Actor = &kusaNear0->actor;
    const int kusaCat = kusaNear0->actor.category;

    // One warmup frame moves each EnKusa from WaitObject into EnKusa_Main.
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, 0);
    const bool nearReady = kusaNear0->actionFunc == EnKusa_Main && kusaNear2->actionFunc == EnKusa_Main;
    Bm1615Check(nearReady && kusaOrigin->actionFunc == EnKusa_Main, "shrubs not in EnKusa_Main", __LINE__);
    Bm1615Check(gSaveContext.ship.stats.count[COUNT_BUSHES_CUT] == 0, "bushes cut nonzero before blast", __LINE__);

    // Press B: ShouldActorUpdate eats BTN_B, arms Default 310 cooldown, and spawns
    // EnBom at world.pos; OoT_EnBom_Update transitions it to BOMB_EXPLOSION that same frame.
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    const bool ateBtnB = (sBm1615Play.state.input[0].press.button & BTN_B) == 0;
    Bm1615Check(ateBtnB, "worn Blast Mask left BTN_B in input", __LINE__);
    Bm1615Check(OoT_BlastMask_CooldownLeft() == 310u, "Blast Mask did not arm Default 310 cooldown", __LINE__);
    EnBom* bomb = Bm1615FindBomb(&sBm1615Play);
    Bm1615Check(bomb != nullptr, "Blast Mask B press did not spawn ACTOR_EN_BOM", __LINE__);
    if (bomb != nullptr) {
        const Vec3f& hp = bomb->actor.home.pos;
        const Sphere16& sph = bomb->explosionCollider.elements[0].dim.worldSphere;
        std::printf("[TEST] shrub leg bomb: world=(%.1f, %.1f, %.1f) sphere=(%d, %d, %d) params=%d\n",
                    bomb->actor.world.pos.x, bomb->actor.world.pos.y, bomb->actor.world.pos.z, (int)sph.center.x,
                    (int)sph.center.y, (int)sph.center.z, (int)bomb->actor.params);
        Bm1615Check(bomb->actor.params == BOMB_EXPLOSION, "instant bomb did not enter BOMB_EXPLOSION", __LINE__);
        const bool homeOk = hp.x == 240.0f && hp.y == 140.0f && hp.z == -180.0f;
        Bm1615Check(homeOk, "bomb home.pos did not match Link's world.pos", __LINE__);
        const bool sphOk = sph.center.x == 240 && sph.center.y == 140 && sph.center.z == -180;
        Bm1615Check(sphOk, "explosionCollider worldSphere.center was not synced to Link's world.pos", __LINE__);
    }

    for (int f = 0; f < 12; f++) {
        Bm1615StepFrame(&sBm1615Play, &sBm1615Player, 0);
    }

    Bm1615Check(!Bm1615ActorInCat(&sBm1615Play, kusaCat, kusaNear0Actor),
                "ENKUSA_TYPE_0 shrub next to Link was not cut and deleted", __LINE__);
    const bool near2Alive = Bm1615ActorInCat(&sBm1615Play, kusaCat, &kusaNear2->actor);
    const bool near2Flagged = (kusaNear2->actor.flags & ACTOR_FLAG_GRASS_DESTROYED) != 0;
    Bm1615Check(near2Alive && near2Flagged && kusaNear2->actionFunc == OoT_EnKusa_DoNothing,
                "ENKUSA_TYPE_2 shrub next to Link was not cut into ACTOR_FLAG_GRASS_DESTROYED", __LINE__);
    Bm1615Check(gSaveContext.ship.stats.count[COUNT_BUSHES_CUT] == 2, "COUNT_BUSHES_CUT != 2", __LINE__);
    const bool originAlive = Bm1615ActorInCat(&sBm1615Play, kusaCat, &kusaOrigin->actor);
    const bool originUnflagged = (kusaOrigin->actor.flags & ACTOR_FLAG_GRASS_DESTROYED) == 0;
    Bm1615Check(originAlive && originUnflagged && kusaOrigin->actionFunc == EnKusa_Main,
                "control shrub at (0, floorY, 0) was hit (blast centred at the pinned focus.pos)", __LINE__);
    Bm1615Check(Bm1615FindBomb(&sBm1615Play) == nullptr, "exploded bomb did not self-delete", __LINE__);

    // Negative control 1: pressing B while cooldown > 0 eats B without spawning a second bomb.
    EnKusa* kusaCooldown = Bm1615SpawnKusa(264.0f, 140.0f, -180.0f, ENKUSA_TYPE_0);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, 0);
    Bm1615Check(OoT_BlastMask_CooldownLeft() > 0u, "cooldown unexpectedly expired", __LINE__);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    Bm1615Check((sBm1615Play.state.input[0].press.button & BTN_B) == 0,
                "worn Blast Mask did not consume BTN_B during cooldown", __LINE__);
    const bool kusaCooldownIntact = kusaCooldown != nullptr && kusaCooldown->actionFunc == EnKusa_Main;
    Bm1615Check(Bm1615FindBomb(&sBm1615Play) == nullptr && kusaCooldownIntact,
                "B press during active cooldown spawned a bomb or cut a shrub", __LINE__);

    // Negative control 2: mask taken off leaves BTN_B for Link's sword and spawns no bomb.
    OoT_ForeignMask_Use(&sBm1615Player, COMBO_FOREIGN_MASK_MM_BLAST);
    OoT_BlastMask_SetCooldownForTest(0);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    const bool leftBtnUnworn = (sBm1615Play.state.input[0].press.button & BTN_B) == BTN_B;
    Bm1615Check(leftBtnUnworn && Bm1615FindBomb(&sBm1615Play) == nullptr,
                "unworn Blast Mask still intercepted BTN_B or spawned a bomb", __LINE__);

    // Negative control 3: frozen rule off ignores a stored foreignMask byte.
    Bm1615FreezeWorld(0u, 0u);
    OoT_ForeignMask_SetStoredRaw(COMBO_FOREIGN_MASK_MM_BLAST);
    OoT_BlastMask_SetCooldownForTest(0);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    const bool leftBtnRuleOff = (sBm1615Play.state.input[0].press.button & BTN_B) == BTN_B;
    Bm1615Check(leftBtnRuleOff && Bm1615FindBomb(&sBm1615Play) == nullptr,
                "Blast Mask fired with RSBS_XITEM_BLAST_MASK_OOT frozen off", __LINE__);

    Bm1615EndScene(&sBm1615Play);
}

/** LA1..LA3 (#1647): adult Link through the LIVE readers (OoT_ForeignMask_Use,
 *  OoT_ForeignMask_WornDl and the blast hook), whose permission is the Masks
 *  Equippable as Adult enhancement's CVar here (OoT_MasksEquippableAsAdult). */
static void Bm1615RunAdultLegs(std::vector<uint8_t>& thaBuf) {
    const char* const kAdultMasks = CVAR_ENHANCEMENT("AdultMasks");
    Bm1615FreezeWorld(RSBS_XITEM_BLAST_MASK_OOT, 0u);
    Bm1615BeginScene(SCENE_KOKIRI_FOREST, 140, thaBuf);
    Bm1615PlacePlayer(&sBm1615Player, 240.0f, 140.0f, -180.0f, 240.0f, 184.0f, -180.0f);

    // LA1: an adult with nothing allowing it cannot put the mask on, and B stays the sword's.
    CVarClear(kAdultMasks);
    Bm1615ArmWearer(&sBm1615Player, LINK_AGE_ADULT);
    Bm1615Check(OoT_ForeignMask_Worn() == COMBO_FOREIGN_MASK_NONE,
                "LA1 an adult not allowed put the Blast Mask on through the live Use", __LINE__);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    Bm1615Check((sBm1615Play.state.input[0].press.button & BTN_B) == BTN_B && Bm1615FindBomb(&sBm1615Play) == nullptr,
                "LA1 an adult not allowed had B taken or a blast spawned", __LINE__);

    // LA2: allowed, the adult wears it, is drawn it, and blasts with B.
    CVarSetInteger(kAdultMasks, 1);
    Bm1615ArmWearer(&sBm1615Player, LINK_AGE_ADULT);
    Bm1615Check(OoT_ForeignMask_Worn() == COMBO_FOREIGN_MASK_MM_BLAST,
                "LA2 an allowed adult could not put the Blast Mask on through the live Use", __LINE__);
    Bm1615Check(OoT_ForeignMask_WornDl(&sBm1615Player, 1) != nullptr,
                "LA2 an allowed adult wearing the Blast Mask is drawn no list", __LINE__);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    Bm1615Check((sBm1615Play.state.input[0].press.button & BTN_B) == 0 && Bm1615FindBomb(&sBm1615Play) != nullptr,
                "LA2 an allowed adult's B press was not the Blast Mask's (no blast)", __LINE__);
    for (int f = 0; f < 12; f++) {
        Bm1615StepFrame(&sBm1615Play, &sBm1615Player, 0);
    }

    // LA3: the permission withdrawn with the mask on: off before it can arm, the same frame.
    Bm1615ArmWearer(&sBm1615Player, LINK_AGE_ADULT);
    CVarClear(kAdultMasks);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    Bm1615Check(OoT_ForeignMask_Worn() == COMBO_FOREIGN_MASK_NONE,
                "LA3 an adult kept the Blast Mask after the permission was withdrawn", __LINE__);
    Bm1615Check((sBm1615Play.state.input[0].press.button & BTN_B) == BTN_B && Bm1615FindBomb(&sBm1615Play) == nullptr,
                "LA3 a withdrawn adult's B press still blasted", __LINE__);

    Bm1615EndScene(&sBm1615Play);
}

static void Bm1615RunGuardHouseSpot(std::vector<uint8_t>& thaBuf, s16 floorY, f32 linkX, f32 linkZ, f32 targetPotX,
                                    f32 targetPotZ) {
    Bm1615FreezeWorld(RSBS_XITEM_BLAST_MASK_OOT, 0u);
    Bm1615BeginScene(SCENE_MARKET_GUARD_HOUSE, floorY, thaBuf);
    Bm1615ArmWearer(&sBm1615Player);
    // In play focus.pos follows Link's head on every drawn frame; it is pinned at
    // (0, 44, 0) here only so a blast spawned at focus.pos is observable, while
    // Link stands at non-origin Guard House coords.
    Bm1615PlacePlayer(&sBm1615Player, linkX, (f32)floorY, linkZ, 0.0f, 44.0f, 0.0f);

    // (16, floorY, -6) is Guard House Child Pot 25, beside the room origin among
    // the pots that ring it (ShufflePots.cpp, RC_MK_GUARD_HOUSE_CHILD_POT_25). At
    // floorY 0 it also catches a sphere left at (0, 0, 0); at floorY -120 it only
    // catches a blast centred at the pinned focus.pos.
    ObjTsubo* potOrigin = Bm1615SpawnPot(16.0f, (f32)floorY, -6.0f);
    ObjTsubo* potNear = Bm1615SpawnPot(targetPotX, (f32)floorY, targetPotZ);
    Bm1615Check(potOrigin != nullptr && potNear != nullptr, "Guard House ObjTsubo spawn failed", __LINE__);
    if (potOrigin == nullptr || potNear == nullptr) {
        Bm1615EndScene(&sBm1615Play);
        return;
    }
    const int potCat = potNear->actor.category;
    const Actor* const potNearActor = &potNear->actor;
    const Actor* const potOriginActor = &potOrigin->actor;

    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, 0);
    Bm1615StepFrame(&sBm1615Play, &sBm1615Player, BTN_B);
    EnBom* bomb = Bm1615FindBomb(&sBm1615Play);
    Bm1615Check(bomb != nullptr, "Guard House B press did not spawn ACTOR_EN_BOM", __LINE__);
    if (bomb != nullptr) {
        const Vec3f& hp = bomb->actor.home.pos;
        const Sphere16& sph = bomb->explosionCollider.elements[0].dim.worldSphere;
        std::printf("[TEST] Guard House (%.0f, %d, %.0f): bomb=(%.1f, %.1f, %.1f) sphere=(%d, %d, %d)\n", linkX,
                    (int)floorY, linkZ, bomb->actor.world.pos.x, bomb->actor.world.pos.y, bomb->actor.world.pos.z,
                    (int)sph.center.x, (int)sph.center.y, (int)sph.center.z);
        const bool homeOk = hp.x == linkX && hp.y == (f32)floorY && hp.z == linkZ;
        Bm1615Check(homeOk, "Guard House bomb spawned away from Link's world.pos", __LINE__);
        const bool sphOk = sph.center.x == (s16)linkX && sph.center.y == floorY && sph.center.z == (s16)linkZ;
        Bm1615Check(sphOk, "Guard House explosionCollider center did not follow Link's world.pos", __LINE__);
    }

    for (int f = 0; f < 12; f++) {
        Bm1615StepFrame(&sBm1615Play, &sBm1615Player, 0);
    }

    const bool potNearGone = !Bm1615ActorInCat(&sBm1615Play, potCat, potNearActor);
    Bm1615Check(potNearGone && gSaveContext.ship.stats.count[COUNT_POTS_BROKEN] == 1,
                "Guard House pot beside Link did not break", __LINE__);
    Bm1615Check(Bm1615ActorInCat(&sBm1615Play, potCat, potOriginActor),
                "Guard House origin pot (16, -6) broke from far away (blast at the origin or the pinned focus.pos)",
                __LINE__);

    Bm1615EndScene(&sBm1615Play);
}

extern "C" int OoT_BlastMaskTest_ShrubAndGuardHouse(void) {
    sBm1615Failures = 0;

    std::unique_ptr<SaveContext> priorSave = std::make_unique<SaveContext>();
    std::memcpy(priorSave.get(), &gSaveContext, sizeof(SaveContext));
    PlayState* const priorPlay = OoT_gPlayState;
    GameInfo* const priorGameInfo = gGameInfo;
    const GameId priorGame = Context_GetCurrentGame();
    const EffectSsInfo priorEffectSs = OoT_sEffectSsInfo;
    const uint16_t priorCooldown = OoT_BlastMask_CooldownLeft();

    const Bm1615SavedCVar savedNoDrops = Bm1615SaveCVar(CVAR_ENHANCEMENT("NoRandomDrops"));
    const Bm1615SavedCVar savedRandEnemies = Bm1615SaveCVar(CVAR_ENHANCEMENT("RandomizedEnemies"));
    const Bm1615SavedCVar savedStaticRadius = Bm1615SaveCVar(CVAR_ENHANCEMENT("StaticExplosionRadius"));
    const Bm1615SavedCVar savedBombMult = Bm1615SaveCVar(CVAR_CHEAT("BombTimerMultiplier"));
    const Bm1615SavedCVar savedPersist = Bm1615SaveCVar(CVAR_ENHANCEMENT("PersistentMasks"));
    const Bm1615SavedCVar savedDpad = Bm1615SaveCVar(CVAR_ENHANCEMENT("DpadEquips"));
    const Bm1615SavedCVar savedNoCooldown = Bm1615SaveCVar(RSBS_CVAR_MM_MASKS_NO_BLAST_MASK_COOLDOWN);
    const Bm1615SavedCVar savedAdultMasks = Bm1615SaveCVar(CVAR_ENHANCEMENT("AdultMasks"));
    const Bm1615SavedCVar savedTimeless = Bm1615SaveCVar(CVAR_CHEAT("TimelessEquipment"));

    bool ownsActorDB = false;
    if (ActorDB::Instance == nullptr) {
        ActorDB::Instance = new ActorDB();
        ownsActorDB = true;
    }
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    bool ownsArena = false;
    std::vector<uint8_t> arenaBuf;
    if (!ZeldaArena_IsInitalized()) {
        arenaBuf.assign(256 * 1024, 0);
        OoT_ZeldaArena_Init(arenaBuf.data(), arenaBuf.size());
        ownsArena = true;
    }

    // Register BlastMaskOoT's and NameTag's real hooks on GameInteractor before setting
    // aside non-ShouldActorUpdate hook tables, so static hook IDs stay valid.
    OoT_BlastMask_RegisterHooks();
    NameTag_RegisterHooks();

    CVarSetInteger(CVAR_ENHANCEMENT("NoRandomDrops"), 1);
    CVarClear(CVAR_ENHANCEMENT("RandomizedEnemies"));
    CVarClear(CVAR_ENHANCEMENT("StaticExplosionRadius"));
    CVarClear(CVAR_CHEAT("BombTimerMultiplier"));
    CVarClear(CVAR_ENHANCEMENT("PersistentMasks"));
    CVarClear(CVAR_ENHANCEMENT("DpadEquips"));
    CVarClear(RSBS_CVAR_MM_MASKS_NO_BLAST_MASK_COOLDOWN);
    CVarClear(CVAR_ENHANCEMENT("AdultMasks"));
    CVarClear(CVAR_CHEAT("TimelessEquipment"));

    {
        Bm1615HooksAside<GameInteractor::OnVanillaBehavior> vbAside;
        Bm1615HooksAside<GameInteractor::ShouldActorInit> shouldInitAside;
        Bm1615HooksAside<GameInteractor::OnActorInit> onInitAside;
        Bm1615HooksAside<GameInteractor::OnActorSpawn> onSpawnAside;
        Bm1615HooksAside<GameInteractor::OnActorUpdate> onUpdateAside;
        Bm1615HooksAside<GameInteractor::OnActorKill> onKillAside;
        Bm1615HooksAside<GameInteractor::OnActorDestroy> onDestroyAside;

        std::vector<uint8_t> thaBuf;
        Bm1615RunShrubLeg(thaBuf);
        Bm1615RunAdultLegs(thaBuf);
        Bm1615RunGuardHouseSpot(thaBuf, 0, -100.0f, 50.0f, -80.0f, 50.0f);
        Bm1615RunGuardHouseSpot(thaBuf, -120, 168.0f, -210.0f, 168.0f, -190.0f);
    }

    Audio_ResetSounds();
    OoT_sEffectSsInfo = priorEffectSs;
    if (ownsArena) {
        OoT_ZeldaArena_Cleanup();
    }
    if (ownsActorDB) {
        delete ActorDB::Instance;
        ActorDB::Instance = nullptr;
    }

    Bm1615RestoreCVar(savedNoDrops);
    Bm1615RestoreCVar(savedRandEnemies);
    Bm1615RestoreCVar(savedStaticRadius);
    Bm1615RestoreCVar(savedBombMult);
    Bm1615RestoreCVar(savedPersist);
    Bm1615RestoreCVar(savedDpad);
    Bm1615RestoreCVar(savedNoCooldown);
    Bm1615RestoreCVar(savedAdultMasks);
    Bm1615RestoreCVar(savedTimeless);

    OoT_BlastMask_SetCooldownForTest(priorCooldown);
    std::memcpy(&gSaveContext, priorSave.get(), sizeof(SaveContext));
    OoT_gPlayState = priorPlay;
    gGameInfo = priorGameInfo;
    Context_SetCurrentGame(priorGame);
    ComboContext_Init();

    return sBm1615Failures == 0 ? 0 : 1;
}

#else

extern "C" int OoT_BlastMaskTest_ShrubAndGuardHouse(void) {
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
