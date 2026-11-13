// RANDOTODO: Consider breaking this up into the different actors
// That speak these hints, like 2Ship does with ActorBehavior

/**
 * This file handles the custom messages for merchants,
 * including both shops and one-off merchants (i.e. Medigoron, Bean Guy,
 * and Carpet Salesman)
 */
#include <cstdio>
#include <string>
#include <soh/OTRGlobals.h>
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "foreign_items.h" // src/common — Combo_GetForeignPlacementForOoTCheck, Combo_GetForeignItemName (#800)
extern "C" int OoT_Rando_Foreign_HostCollected(uint16_t rc); // ForeignItemsSingleExe.cpp (#800)
extern "C" uint16_t OoT_Rando_ShopShelfDisplayedItem(uint16_t rc, int bought); // ShopRestockSingleExe.cpp (#1547)
extern "C" int OoT_Rando_ShopMessageIncludesOneLeft(uint16_t rc, int bought, int mysterious);
static bool BuildForeignHostMessage(CustomMessage& msg, RandomizerCheck rc);
#endif

extern "C" {
extern PlayState* OoT_gPlayState;
#include <macros.h>
#include <functions.h>
#include <variables.h>
#include <overlays/actors/ovl_En_Dns/z_en_dns.h>
}

#define RAND_GET_OPTION(rsk) OTRGlobals::Instance->gRandoContext->GetOption(rsk)
#define RAND_GET_ITEM(rc) OTRGlobals::Instance->gRandoContext->GetItemLocation(rc)
#define RAND_GET_OVERRIDE(rc) OTRGlobals::Instance->gRandoContext->overrides[rc]
#define NON_BEAN_MERCHANTS                                                            \
    (RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL_BUT_BEANS) || \
     RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL))

static Rando::Context* MerchantRandoContext() {
    if (OTRGlobals::Instance != nullptr && OTRGlobals::Instance->gRandoContext != nullptr) {
        return OTRGlobals::Instance->gRandoContext.get();
    }
    return Rando::Context::GetInstance().get();
}

void BuildMerchantMessage(CustomMessage& msg, RandomizerCheck rc, bool mysterious = true,
                          RandomizerGet displayedOverride = RG_NONE) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 pass 2: a merchant (the bean salesman, Medigoron, Granny, the carpet
    // salesman) hosting an MM item names that item where it would name its own
    // (Merchant Hint Text on, Mysterious Shuffle off), not the junk cover. The
    // shelf and the scrub take the same branch before they get here.
    if (!mysterious && BuildForeignHostMessage(msg, rc)) {
        return;
    }
#endif
    Rando::Context* ctx = MerchantRandoContext();
    RandomizerGet rgid =
        displayedOverride != RG_NONE ? displayedOverride : ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
    uint16_t price = ctx->GetItemLocation(rc)->GetPrice();
    CustomMessage itemName;
    std::string color = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetColor();
    if (mysterious) {
        itemName = Rando::StaticData::hintTextTable[RHT_MYSTERIOUS_ITEM_CAPITAL].GetHintMessage();
        color = "%g";
    } else if (rgid == RG_ICE_TRAP) {
        rgid = ctx->overrides[rc].LooksLike();
        itemName = CustomMessage(ctx->overrides[rc].GetTrickName());
        color = "%g";
    } else {
        itemName = CustomMessage(Rando::StaticData::RetrieveItem(rgid).GetName());
    }
    msg.Replace("[[color]]", color);
    msg.InsertNames({ itemName, CustomMessage(std::to_string(price)) });
}

#ifdef RSBS_SINGLE_EXECUTABLE
// #800: when OoT check `rc` hosts a Majora's Mask item, name that item, not the
// junk cover the OoT table holds there, at the host's own price. The name comes
// from MM's describer (the same one the drain's pickup toast reads), in English
// only, and with OoT's default item colour. false: no MM item at `rc`, or no
// live pairing (the drain then refuses the crossing and gives the cover, so the
// host names the cover, as the Check Tracker's PlacedItemTrackerName does).
static bool BuildForeignHostMessage(CustomMessage& msg, RandomizerCheck rc) {
    if (!Combo_ForeignPairingActive()) {
        return false;
    }
    const SharedItem* foreignItem = Combo_GetForeignPlacementForOoTCheck((uint16_t)rc);
    if (foreignItem == nullptr) {
        return false;
    }
    // #1481: named as it will be granted (a Progressive Time as its next
    // half-day); not bought yet, so its own crossing is not in the array.
    const char* foreignName = Combo_GetForeignItemName(Combo_PresentForeignItem(*foreignItem, false));
    msg.Replace("[[color]]", "%g");
    msg.InsertNames({ CustomMessage(std::string(foreignName != nullptr ? foreignName : "Majora's Mask item")),
                      CustomMessage(std::to_string(MerchantRandoContext()->GetItemLocation(rc)->GetPrice())) });
    return true;
}
#endif

void BuildBeanGuyMessage(uint16_t* textId, bool* loadFromMessageTable) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 pass 2: his MM item was already bought (his sale hook refuses the
    // sale and says he is sold out), but BEANS_BOUGHT was lost with an unsaved
    // reload, so he offers again: his vanilla sold-out text instead, as once
    // BEANS_BOUGHT is 10. It has no choice, so his talk ends as it does then.
    if (*textId == TEXT_BEAN_SALESMAN_BUY_FOR_10 && OoT_Rando_Foreign_HostCollected(RC_ZR_MAGIC_BEAN_SALESMAN)) {
        *textId = TEXT_BEAN_SALESMAN_SOLD_OUT;
        return;
    }
#endif
    CustomMessage msg;
    if (*textId == TEXT_BEAN_SALESMAN_BUY_FOR_100) {
        msg = CustomMessage(
            "I never thought I'd say this, but I'm selling the last %rMagic Bean%w.^%y99 Rupees%w, no "
            "less.\x1B%gYes&No%w",
            "Ich hätte nie gedacht, daß ich das sage, aber ich verkaufe die letzte^%rWundererbse%w für %y99 "
            "Rubine%w.\x1B&%gJa&Nein%w",
            "Je te vends mon dernier %rHaricot&magique%g pour %y99 Rubis%w.\x1B&%gAcheterNe pas acheter%w");
        msg.Format();
    } else if (*textId == TEXT_BEAN_SALESMAN_BUY_FOR_10) {
        msg = CustomMessage("Want to buy [[color]][[1]]%w for %y[[2]] Rupees%w?\x1B%gYes&No%w",
                            "Möchten Sie [[color]][[1]]%w für %y[[2]] Rubin%w kaufen?\x1B%gJa&Nein%w",
                            "Voulez-vous acheter [[color]][[1]]%w pour %y[[2]] Rubis%w?\x1B%gOui&Non%w");
        BuildMerchantMessage(msg, RC_ZR_MAGIC_BEAN_SALESMAN,
                             RAND_GET_OPTION(RSK_MERCHANT_TEXT_HINT).Is(RO_GENERIC_OFF) ||
                                 CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0));
        msg.AutoFormat();
    }
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void BuildMedigoronMessage(uint16_t* textId, bool* loadFromMessageTable) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 pass 2: his MM item was already bought (EnGm_RandoCanGetMedigoronItem
    // refuses the sale, so he sells his vanilla Giant's Knife): his vanilla offer.
    if (OoT_Rando_Foreign_HostCollected(RC_GC_MEDIGORON)) {
        return;
    }
#endif
    CustomMessage msg = CustomMessage("Want to buy [[color]][[1]]%w for %y[[2]] Rupees%w?\x1B%gYes&No%w",
                                      "Möchten Sie [[color]][[1]]%w für %y[[2]] Rubin%w kaufen?\x1B%gJa&Nein%w",
                                      "Voulez-vous acheter [[color]][[1]]%w pour %y[[2]] Rubis%w?\x1B%gOui&Non%w");
    BuildMerchantMessage(msg, RC_GC_MEDIGORON,
                         RAND_GET_OPTION(RSK_MERCHANT_TEXT_HINT).Is(RO_GENERIC_OFF) ||
                             CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0));
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void BuildGrannyMessage(uint16_t* textId, bool* loadFromMessageTable) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 pass 2: her MM item was already bought (EnDs_RandoCanGetGrannyItem
    // refuses the sale): her vanilla offer, as once the flag is set.
    if (OoT_Rando_Foreign_HostCollected(RC_KAK_GRANNYS_SHOP)) {
        return;
    }
#endif
    // [RSBS #1414] Early Granny's Potion Shop (only read with Shuffle Adult Trade off,
    // the first term) stands in for the Claim Check, as at her offer
    // (VB_OFFER_BLUE_POTION, hook_handlers.cpp); missing upstream too, inherited,
    // fixed here.
    if (!Flags_GetRandomizerInf(RAND_INF_MERCHANTS_GRANNYS_SHOP) &&
        (RAND_GET_OPTION(RSK_SHUFFLE_ADULT_TRADE).Is(RO_GENERIC_ON) ||
         RAND_GET_OPTION(RSK_EARLY_GRANNYS_SHOP).Is(RO_GENERIC_ON) ||
         INV_CONTENT(ITEM_CLAIM_CHECK) == ITEM_CLAIM_CHECK)) {
        CustomMessage msg = CustomMessage("Want to buy [[color]][[1]]%w for %y[[2]] Rupees%w?\x1B%gYes&No%w",
                                          "Möchten Sie [[color]][[1]]%w für %y[[2]] Rubin%w kaufen?\x1B%gJa&Nein%w",
                                          "Voulez-vous acheter [[color]][[1]]%w pour %y[[2]] Rubis%w?\x1B%gOui&Non%w");
        BuildMerchantMessage(msg, RC_KAK_GRANNYS_SHOP,
                             RAND_GET_OPTION(RSK_MERCHANT_TEXT_HINT).Is(RO_GENERIC_OFF) ||
                                 CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0));
        msg.AutoFormat();
        msg.LoadIntoFont();
        *loadFromMessageTable = false;
    }
}

void BuildCarpetGuyMessage(uint16_t* textId, bool* loadFromMessageTable) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 pass 2: his MM item was already bought (EnJs_RandoCanGetCarpetMerchantItem
    // refuses the sale, so he sells his vanilla Bombchus): his vanilla offer.
    if (*textId != TEXT_CARPET_SALESMAN_ARMS_DEALER &&
        OoT_Rando_Foreign_HostCollected(RC_WASTELAND_BOMBCHU_SALESMAN)) {
        return;
    }
#endif
    CustomMessage msg;
    if (*textId == TEXT_CARPET_SALESMAN_ARMS_DEALER) {
        msg = CustomMessage("Finally! Now I can go back to being an %rarms dealer%w!",
                            /*german*/ "Endlich! Schon bald kann ich wieder %rKrabbelminen-Händler%w sein!",
                            /*french*/ "Squalala! Je vais enfin pouvoir %rprendre des vacances%w!");
    } else if (!Flags_GetRandomizerInf(RAND_INF_MERCHANTS_CARPET_SALESMAN)) {
        msg = CustomMessage("Want to buy [[color]][[1]]%w for %y[[2]] Rupees%w?\x1B%gYes&No%w",
                            "Möchten Sie [[color]][[1]]%w für %y[[2]] Rubin%w kaufen?\x1B%gJa&Nein%w",
                            "Voulez-vous acheter [[color]][[1]]%w pour %y[[2]] Rubis%w?\x1B%gOui&Non%w");
        BuildMerchantMessage(msg, RC_WASTELAND_BOMBCHU_SALESMAN,
                             RAND_GET_OPTION(RSK_MERCHANT_TEXT_HINT).Is(RO_GENERIC_OFF) ||
                                 CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0));
    }
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void BuildCarpetGuyFailToBuyMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg =
        CustomMessage("I'm sorry I can't sell you these fine specimens, they need an %rexperienced owner%w.^"
                      "Come back when you have had %gBombchus%w of your own.");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// The scrub's offer for check `rc`, formatted and ready to load. Split out of
// BuildScrubMessage so a ROM-free row can read the text a given pair of hint
// settings produces (OoT_Rando_ScrubOfferText below).
static CustomMessage BuildScrubOfferMessage(RandomizerCheck rc) {
    uint16_t price = RAND_GET_ITEM(rc)->GetPrice();
    CustomMessage msg;
    if (price == 0) {
        msg = CustomMessage("\x12\x38\x82"
                            "All right! You win! In return for sparing me, I will give you a [[color]][[1]]%w!&Please, "
                            "take it!\x07\x10\xA3",
                            "\x12\x38\x82"
                            "In Ordnung! Du gewinnst! Im Austausch dafür, dass Du mich verschont hast, werde ich Dir "
                            "einen [[color]][[1]]%w geben!\x07\x10\xA3",
                            "\x12\x38\x82"
                            "J'me rends! Laisse-moi partir et en échange, je te donne un [[color]][[1]]%w! Vas-y "
                            "prends le!\x07\x10\xA3");
    } else {
        msg = CustomMessage(
            "\x12\x38\x82"
            "All right! You win! In return for sparing me, I will sell you a [[color]][[1]]%w! %y[[2]] Rupees%w it "
            "is!\x07\x10\xA3",
            "\x12\x38\x82"
            "Ich gebe auf! Ich verkaufe Dir einen [[color]][[1]]%w für %y[[2]] Rubine%w!\x07\x10\xA3",
            "\x12\x38\x82"
            "J'abandonne! Tu veux bien m'acheter un [[color]][[1]]%w? Ça fera %y[[2]] Rubis%w!\x07\x10\xA3");
    }
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 pass 2: a scrub hosting an MM item names that item where it would name
    // its own (Scrub Hint Text on, Mysterious Shuffle off), not the junk cover.
    if (!RAND_GET_OPTION(RSK_SCRUB_TEXT_HINT).Is(RO_GENERIC_OFF) &&
        !CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0) && BuildForeignHostMessage(msg, rc)) {
        msg.AutoFormat();
        return msg;
    }
#endif
    // #855: the scrub's offer follows Scrub Hint Text ("Business scrubs will reveal the identity of what
    // they're selling"), the setting the Check Tracker's reveal and row name read for a scrub. Upstream read
    // Merchant Hint Text here, so the box and the tracker disagreed whenever the two settings differed;
    // Merchant Hint Text keeps governing the other merchants.
    BuildMerchantMessage(msg, rc,
                         RAND_GET_OPTION(RSK_SCRUB_TEXT_HINT).Is(RO_GENERIC_OFF) ||
                             CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0));
    msg.AutoFormat();
    return msg;
}

#ifdef RSBS_SINGLE_EXECUTABLE
// Test seam (row ScrubTextHintSetting, #855): the English text of the scrub offer
// for check `rc` under the live hint settings. Returns its length (it is cut to
// `cap` - 1 characters when longer), or -1 for a bad argument.
extern "C" int OoT_Rando_ScrubOfferText(uint16_t rc, char* out, size_t cap) {
    if (out == nullptr || cap == 0) {
        return -1;
    }
    const std::string text = BuildScrubOfferMessage(static_cast<RandomizerCheck>(rc)).GetEnglish();
    std::snprintf(out, cap, "%s", text.c_str());
    return static_cast<int>(text.size());
}
#endif

void BuildScrubMessage(uint16_t* textId, bool* loadFromMessageTable) {
    EnDns* enDns = reinterpret_cast<EnDns*>(GET_PLAYER(OoT_gPlayState)->talkActor);
    RandomizerCheck rc = ObjectExtension::GetInstance().Get<ScrubIdentity>(enDns)->identity.randomizerCheck;
    CustomMessage msg = BuildScrubOfferMessage(rc);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

static CustomMessage BuildShopOfferMessage(RandomizerCheck rc, bool bought, bool confirm, bool mysterious) {
    CustomMessage msg;
    if (confirm) {
        msg = CustomMessage("\x08[[color]][[1]]%w  %y[[2]]_Rupees%w\x09\x1B%gBuy&Don't buy%w\x09\x02",
                            "\x08[[color]][[1]]%w  %y[[2]]_Rubine%w\x09\x1B%gKaufen&Nicht kaufen%w\x09\x02",
                            "\x08[[color]][[1]]%w  %y[[2]]_Rubis%w\x09\x1B%gAcheter&Ne pas acheter%w\x09\x02");
    } else {
        bool includeOneLeft = true;
#ifdef RSBS_SINGLE_EXECUTABLE
        includeOneLeft =
            OoT_Rando_ShopMessageIncludesOneLeft(static_cast<uint16_t>(rc), bought ? 1 : 0, mysterious ? 1 : 0) != 0;
#endif
        if (includeOneLeft) {
            msg = CustomMessage(
                "\x08[[color]][[1]]%w  %y[[2]]_Rupees%w&Special deal! %rONE LEFT%w!\x0A\x02",
                "\x08[[color]][[1]]%w  %y[[2]]_Rubine%w&Sonderangebot! %rNUR NOCH EINES VERFÜGBAR%w!\x0A\x02",
                "\x08[[color]][[1]]%w  %y[[2]]_Rubis%w&Offre spéciale! %rDERNIER EN STOCK%w!\x0A\x02");
        } else {
            msg = CustomMessage("\x08[[color]][[1]]%w  %y[[2]]_Rupees%w\x0A\x02",
                                "\x08[[color]][[1]]%w  %y[[2]]_Rubine%w\x0A\x02",
                                "\x08[[color]][[1]]%w  %y[[2]]_Rubis%w\x0A\x02");
        }
    }
#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 S1: a shelf hosting a Majora's Mask item names that item.
    if (!mysterious && BuildForeignHostMessage(msg, rc)) {
        msg.AutoFormat();
        return msg;
    }
#endif
    RandomizerGet displayed = RG_NONE;
#ifdef RSBS_SINGLE_EXECUTABLE
    if (bought) {
        displayed = static_cast<RandomizerGet>(OoT_Rando_ShopShelfDisplayedItem(static_cast<uint16_t>(rc), 1));
    }
#endif
    BuildMerchantMessage(msg, rc, mysterious, displayed);
    msg.AutoFormat();
    return msg;
}

/** The shop offer for message @p textId: which shelf (its RandomizerInf, the
 *  shelf check @p checkForInf maps it to), whether it is the confirm box, and
 *  whether the shelf is bought (its RandomizerInf, read here). @return false
 *  when @p textId is not a shop offer. */
static bool BuildShopMessageFor(uint16_t textId, RandomizerCheck (*checkForInf)(RandomizerInf), bool mysterious,
                                CustomMessage& out) {
    RandomizerInf shopInf;
    bool confirm = false;
    // Shop items each have two message entries, second one offset by NUM_SHOP_ITEMS
    // textId: TEXT_SHOP_ITEM_RANDOM + (randomizerInf - RAND_INF_SHOP_ITEMS_KF_SHOP_ITEM_1)
    // textId: TEXT_SHOP_ITEM_RANDOM + ((randomizerInf - RAND_INF_SHOP_ITEMS_KF_SHOP_ITEM_1) + NUM_SHOP_ITEMS)
    if (textId >= TEXT_SHOP_ITEM_RANDOM && textId < TEXT_SHOP_ITEM_RANDOM_CONFIRM) {
        shopInf = static_cast<RandomizerInf>((textId - TEXT_SHOP_ITEM_RANDOM) + RAND_INF_SHOP_ITEMS_KF_SHOP_ITEM_1);
    } else if (textId >= TEXT_SHOP_ITEM_RANDOM_CONFIRM && textId <= TEXT_SHOP_ITEM_RANDOM_CONFIRM_END) {
        shopInf =
            static_cast<RandomizerInf>((textId - TEXT_SHOP_ITEM_RANDOM_CONFIRM) + RAND_INF_SHOP_ITEMS_KF_SHOP_ITEM_1);
        confirm = true;
    } else {
        return false;
    }
    const RandomizerCheck rc = checkForInf(shopInf);
    out = BuildShopOfferMessage(rc, Flags_GetRandomizerInf(shopInf) != 0, confirm, mysterious);
    return true;
}

static RandomizerCheck ShopCheckFromRandomizerInf(RandomizerInf shopInf) {
    return OTRGlobals::Instance->gRandomizer->GetCheckFromRandomizerInf(shopInf);
}

#ifdef RSBS_SINGLE_EXECUTABLE
/** English text of @p msg with OoT control codes stripped into @p out. */
static int ShopOfferPlainText(const CustomMessage& msg, char* out, size_t cap) {
    const std::string raw = msg.GetEnglish(MF_RAW);
    // Strip OoT control codes (importantly 0x05 + QM_WHITE '\0', which would
    // terminate a C string before "ONE LEFT!" / "Buy") so out is printable.
    std::string text;
    text.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c == 0x05 || c == 0x13) {
            if (i + 1 < raw.size()) {
                i++;
            }
        } else if (c == 0x01 || c == 0x04) {
            text.push_back('\n');
        } else if (c >= 0x20 && c < 0x7F) {
            text.push_back(static_cast<char>(c));
        }
    }
    std::snprintf(out, cap, "%s", text.c_str());
    return static_cast<int>(text.size());
}

/** Test bridge: the offer for shelf @p rc with @p bought given directly. */
extern "C" int OoT_Rando_ShopOfferText(uint16_t rc, int bought, int confirm, int mysterious, char* out, size_t cap) {
    if (out == nullptr || cap == 0 || MerchantRandoContext() == nullptr) {
        return -1;
    }
    return ShopOfferPlainText(
        BuildShopOfferMessage(static_cast<RandomizerCheck>(rc), bought != 0, confirm != 0, mysterious != 0), out, cap);
}

static RandomizerCheck sShopOfferTestCheck = RC_UNKNOWN_CHECK;

/** Test bridge: the offer for message @p textId through BuildShopMessageFor,
 *  the production path, which reads the shelf's RandomizerInf from the live save
 *  itself; @p rc stands in for the randomizer's RandomizerInf -> check map.
 *  @return -1 when @p textId is not a shop offer. */
extern "C" int OoT_Rando_ShopOfferTextForTextId(uint16_t textId, uint16_t rc, int mysterious, char* out, size_t cap) {
    if (out == nullptr || cap == 0 || MerchantRandoContext() == nullptr) {
        return -1;
    }
    sShopOfferTestCheck = static_cast<RandomizerCheck>(rc);
    CustomMessage msg;
    if (!BuildShopMessageFor(
            textId, [](RandomizerInf) { return sShopOfferTestCheck; }, mysterious != 0, msg)) {
        return -1;
    }
    return ShopOfferPlainText(msg, out, cap);
}
#endif

void BuildShopMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg;
    if (!BuildShopMessageFor(*textId, ShopCheckFromRandomizerInf,
                             CVarGetInteger(CVAR_RANDOMIZER_ENHANCEMENT("MysteriousShuffle"), 0) != 0, msg)) {
        return;
    }
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterMerchantMessages() {
    COND_ID_HOOK(OnOpenText, TEXT_BEAN_SALESMAN_BUY_FOR_10,
                 (RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_BEANS_ONLY) ||
                  RAND_GET_OPTION(RSK_SHUFFLE_MERCHANTS).Is(RO_SHUFFLE_MERCHANTS_ALL)),
                 BuildBeanGuyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_BEAN_SALESMAN_BUY_FOR_100, IS_RANDO, BuildBeanGuyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_MEDIGORON, NON_BEAN_MERCHANTS, BuildMedigoronMessage);
    COND_ID_HOOK(OnOpenText, TEXT_GRANNYS_SHOP, NON_BEAN_MERCHANTS, BuildGrannyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_CARPET_SALESMAN_1, NON_BEAN_MERCHANTS, BuildCarpetGuyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_CARPET_SALESMAN_ARMS_DEALER, NON_BEAN_MERCHANTS, BuildCarpetGuyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_SCRUB_RANDOM, IS_RANDO, BuildScrubMessage);
    COND_HOOK(OnOpenText, RAND_GET_OPTION(RSK_SHOPSANITY).IsNot(RO_SHOPSANITY_OFF), BuildShopMessage);
    COND_ID_HOOK(OnOpenText, TEXT_CARPET_SALESMAN_CUSTOM_FAIL_TO_BUY, IS_RANDO, BuildCarpetGuyFailToBuyMessage);
}

static RegisterShipInitFunc initFunc(RegisterMerchantMessages, { "IS_RANDO" });