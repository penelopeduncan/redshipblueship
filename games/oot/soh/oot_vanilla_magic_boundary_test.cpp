/**
 * @file oot_vanilla_magic_boundary_test.cpp
 * @brief ROM-free, display-free regression lock for #1588 (SoH f93e3cfb #7415,
 *        41ae6cd3 #7422): OoT infinite-magic and Barinade rematch-intro checks
 *        stay behind OnVanillaBehavior hooks instead of reading randomizer state
 *        in vanilla code, and GameInteractor::RemoveAllQueuedHooks drains queued
 *        unregistrations from its out-of-line GameInteractor.cpp definition.
 *
 * CTest label "redship", row OoTVanillaMagicBoundary in CMake/SingleExecutable.cmake,
 * dispatch "oot-vanilla-magic-boundary" in src/common/test_runner.cpp.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <libultraship/bridge.h>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "game.h"

#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

extern "C" {
extern SaveContext gSaveContext;
s32 Flags_GetRandomizerInf(RandomizerInf flag);
void Flags_SetRandomizerInf(RandomizerInf flag);
void Flags_UnsetRandomizerInf(RandomizerInf flag);
}

// hook_handlers.cpp: registered on randomizer file load by RandomizerRegisterHooks.
void RandomizerOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list originalArgs);

namespace {

const char* kMagicInfChangedCvar = CVAR_COSMETIC("Consumable_MagicInfinite.Changed");
const char* kMagicInfValueCvar = CVAR_COSMETIC("Consumable_MagicInfinite.Value");

int sFailures = 0;

static void VmbFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-vanilla-magic-boundary: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

static void VmbCheck(bool cond, const char* msg) {
    if (!cond) {
        VmbFail("%s", msg);
    }
}

struct VmbSavedCVar {
    std::string name;
    bool existed = false;
    Ship::ConsoleVariableType type = Ship::ConsoleVariableType::Integer;
    int32_t integer = 0;
    float real = 0.0f;
    std::string text;
    Color_RGBA8 color = {};
    Color_RGB8 color24 = {};
};

static VmbSavedCVar VmbSaveCVar(const char* name) {
    VmbSavedCVar saved;
    saved.name = name;
    const std::shared_ptr<Ship::CVar> cvar = CVarGet(name);
    if (cvar == nullptr) {
        return saved;
    }
    saved.existed = true;
    saved.type = cvar->Type;
    switch (cvar->Type) {
        case Ship::ConsoleVariableType::Integer:
            saved.integer = cvar->Integer;
            break;
        case Ship::ConsoleVariableType::Float:
            saved.real = cvar->Float;
            break;
        case Ship::ConsoleVariableType::String:
            if (cvar->String != nullptr) {
                saved.text = cvar->String;
            }
            break;
        case Ship::ConsoleVariableType::Color:
            saved.color = cvar->Color;
            break;
        case Ship::ConsoleVariableType::Color24:
            saved.color24 = cvar->Color24;
            break;
    }
    return saved;
}

static void VmbRestoreCVar(const VmbSavedCVar& saved) {
    if (!saved.existed) {
        CVarClear(saved.name.c_str());
        return;
    }
    switch (saved.type) {
        case Ship::ConsoleVariableType::Integer:
            CVarSetInteger(saved.name.c_str(), saved.integer);
            break;
        case Ship::ConsoleVariableType::Float:
            CVarSetFloat(saved.name.c_str(), saved.real);
            break;
        case Ship::ConsoleVariableType::String:
            CVarSetString(saved.name.c_str(), saved.text.c_str());
            break;
        case Ship::ConsoleVariableType::Color:
            CVarSetColor(saved.name.c_str(), saved.color);
            break;
        case Ship::ConsoleVariableType::Color24:
            CVarSetColor24(saved.name.c_str(), saved.color24);
            break;
    }
}

static std::string VmbReadSourceNormalized(const char* sourceDir, const char* relPath) {
    if (sourceDir == nullptr || sourceDir[0] == '\0') {
        return std::string();
    }
    const std::string fullPath = std::string(sourceDir) + "/" + relPath;
    std::ifstream in(fullPath, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    out.reserve(raw.size());
    for (const char ch : raw) {
        if (ch != '\r') {
            out.push_back(ch);
        }
    }
    return out;
}

// Replace C and C++ comments with spaces while keeping string and character literals intact.
static std::string VmbStripCommentsKeepStrings(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    for (size_t pos = 0; pos < src.size(); pos++) {
        const char c = src[pos];
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            const size_t eol = src.find('\n', pos + 2);
            out.push_back(' ');
            if (eol == std::string::npos) {
                break;
            }
            pos = eol - 1;
            continue;
        }
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '*') {
            const size_t end = src.find("*/", pos + 2);
            out.push_back(' ');
            if (end == std::string::npos) {
                break;
            }
            pos = end + 1;
            continue;
        }
        if (c == '"' || c == '\'') {
            const char quote = c;
            out.push_back(quote);
            pos++;
            while (pos < src.size()) {
                out.push_back(src[pos]);
                if (src[pos] == '\\' && pos + 1 < src.size()) {
                    pos++;
                    out.push_back(src[pos]);
                    pos++;
                    continue;
                }
                if (src[pos] == quote) {
                    break;
                }
                pos++;
            }
            continue;
        }
        out.push_back(c);
    }
    return out;
}

static std::string VmbCollapseWhitespace(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool inSpace = false;
    for (const char c : src) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!inSpace) {
                out.push_back(' ');
                inSpace = true;
            }
        } else {
            out.push_back(c);
            inSpace = false;
        }
    }
    return out;
}

static size_t VmbSkipSpace(const std::string& src, size_t pos) {
    while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n')) {
        pos++;
    }
    return pos;
}

static std::string VmbExtractBraceBlock(const std::string& strippedSrc, size_t openBracePos) {
    if (openBracePos >= strippedSrc.size() || strippedSrc[openBracePos] != '{') {
        return std::string();
    }
    int depth = 0;
    for (size_t pos = openBracePos; pos < strippedSrc.size(); pos++) {
        const char c = strippedSrc[pos];
        if (c == '"' || c == '\'') {
            const char quote = c;
            pos++;
            while (pos < strippedSrc.size()) {
                if (strippedSrc[pos] == '\\') {
                    pos += 2;
                    continue;
                }
                if (strippedSrc[pos] == quote) {
                    break;
                }
                pos++;
            }
            if (pos >= strippedSrc.size()) {
                return std::string();
            }
            continue;
        }
        if (c == '{') {
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                return strippedSrc.substr(openBracePos, pos - openBracePos + 1);
            }
        }
    }
    return std::string();
}

static std::string VmbExtractFunctionBody(const std::string& strippedSrc, const char* signature) {
    const size_t sigLen = std::strlen(signature);
    size_t at = 0;
    while ((at = strippedSrc.find(signature, at)) != std::string::npos) {
        if (at > 0 && strippedSrc[at - 1] != '\n') {
            at += sigLen;
            continue;
        }
        const size_t openBrace = VmbSkipSpace(strippedSrc, at + sigLen);
        if (openBrace < strippedSrc.size() && strippedSrc[openBrace] == '{') {
            return VmbExtractBraceBlock(strippedSrc, openBrace);
        }
        at = openBrace;
    }
    return std::string();
}

static bool VmbContains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static void VmbRunSourceLeg(const char* sourceDir) {
    std::string sample = "void Sample(int x);\nvoid Sample(int x) {\n";
    sample += "    // HIDDEN_LINE_TOKEN\n";
    sample += "    /* HIDDEN_BLOCK_TOKEN */\n";
    sample += "    const char* s = \"LITERAL_TOKEN\";\n";
    sample += "    if (Call(x)) {\n        Use(s);\n    }\n}\n";
    const std::string sampleStripped = VmbStripCommentsKeepStrings(sample);
    const std::string sampleRawBody = VmbExtractFunctionBody(sampleStripped, "void Sample(int x)");
    const std::string sampleBody = VmbCollapseWhitespace(sampleRawBody);
    VmbCheck(!VmbContains(sampleStripped, "HIDDEN_LINE_TOKEN"), "self-check failed to strip // comment");
    VmbCheck(!VmbContains(sampleStripped, "HIDDEN_BLOCK_TOKEN"), "self-check failed to strip /* */ comment");
    VmbCheck(VmbContains(sampleStripped, "LITERAL_TOKEN"), "self-check dropped string literal content");
    VmbCheck(VmbContains(sampleBody, "if (Call(x)) { Use(s); }"), "self-check failed to extract function body");

    const std::string paramRaw = VmbReadSourceNormalized(sourceDir, "games/oot/src/code/z_parameter.c");
    VmbCheck(!paramRaw.empty(), "could not read games/oot/src/code/z_parameter.c");
    if (!paramRaw.empty()) {
        const std::string p = VmbStripCommentsKeepStrings(paramRaw);
        VmbCheck(!VmbContains(p, "RAND_INF_HAS_INFINITE_MAGIC_METER"), "z_parameter.c reads infinite-magic flag");
        VmbCheck(!VmbContains(p, "magicbar_blue"), "z_parameter.c still declares or uses magicbar_blue");
        VmbCheck(!VmbContains(p, "Consumable_MagicInfinite"), "z_parameter.c reads Consumable_MagicInfinite CVar");

        const char* kUpdateSig = "void Interface_UpdateMagicBar(PlayState* play)";
        const std::string updateBody = VmbCollapseWhitespace(VmbExtractFunctionBody(p, kUpdateSig));
        VmbCheck(!updateBody.empty(), "Interface_UpdateMagicBar body not found in z_parameter.c");
        std::string depleteNeedle = "case MAGIC_STATE_CONSUME: ";
        depleteNeedle += "if (GameInteractor_Should(VB_MAGIC_METER_DEPLETE_WHILE_CONSUMING, true)) {";
        VmbCheck(VmbContains(updateBody, depleteNeedle), "MAGIC_STATE_CONSUME missing deplete VB call");

        const char* kDrawSig = "void Interface_DrawMagicBar(PlayState* play)";
        const std::string drawBody = VmbCollapseWhitespace(VmbExtractFunctionBody(p, kDrawSig));
        VmbCheck(!drawBody.empty(), "Interface_DrawMagicBar body not found in z_parameter.c");
        const char* kColorNeedle = "GameInteractor_Should(VB_APPLY_MAGIC_METER_FILL_COLOR, true, &magicbar_green);";
        VmbCheck(VmbContains(drawBody, kColorNeedle), "Interface_DrawMagicBar missing fill-color VB call");
    }

    const char* kBossVaPath = "games/oot/src/overlays/actors/ovl_Boss_Va/z_boss_va.c";
    const std::string bossVaRaw = VmbReadSourceNormalized(sourceDir, kBossVaPath);
    VmbCheck(!bossVaRaw.empty(), "could not read games/oot/src/overlays/actors/ovl_Boss_Va/z_boss_va.c");
    if (!bossVaRaw.empty()) {
        const std::string b = VmbStripCommentsKeepStrings(bossVaRaw);
        VmbCheck(!VmbContains(b, "RSK_SHUFFLE_BOSS_SOULS"), "z_boss_va.c still reads RSK_SHUFFLE_BOSS_SOULS");
        VmbCheck(!VmbContains(b, "RAND_INF_BARINADE_SOUL"), "z_boss_va.c still reads RAND_INF_BARINADE_SOUL");
        VmbCheck(!VmbContains(b, "Randomizer_GetSettingValue"), "z_boss_va.c calls Randomizer_GetSettingValue");

        const char* kBossInitSig = "void BossVa_Init(Actor* thisx, PlayState* play2)";
        const std::string initBody = VmbCollapseWhitespace(VmbExtractFunctionBody(b, kBossInitSig));
        VmbCheck(!initBody.empty(), "BossVa_Init body not found in z_boss_va.c");
        std::string barinadeNeedle = "if (GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, ";
        barinadeNeedle += "OoT_Flags_GetEventChkInf(EVENTCHKINF_BEGAN_BARINA_BATTLE), this)) {";
        VmbCheck(VmbContains(initBody, barinadeNeedle), "BossVa_Init missing Barinade rematch VB call");
        const char* kElseNeedle = "} else { sCsState = INTRO_START; sDoorState = 5; }";
        VmbCheck(VmbContains(initBody, kElseNeedle), "BossVa_Init else branch missing INTRO_START");
    }
}

static bool VmbSameRgb8(Color_RGB8 a, Color_RGB8 b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

static void VmbRunHandlerLeg(Rando::Context& randoCtx) {
    using OnVb = GameInteractor::OnVanillaBehavior;
    const Color_RGB8 kVanillaGreen = { 0, 200, 0 };
    const Color_RGB8 kDefaultInfBlue = { 0, 0, 200 };
    const Color_RGB8 kCustomInfColor = { 12, 34, 56 };

    // (a) Vanilla control: no RandomizerOnVanillaBehaviorHandler registered.
    // Even if RAND_INF_HAS_INFINITE_MAGIC_METER is left set in gSaveContext,
    // vanilla magic still depletes and keeps its green fill color (#7415).
    Flags_SetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER);
    CVarSetInteger(kMagicInfChangedCvar, 1);
    CVarSetColor24(kMagicInfValueCvar, kCustomInfColor);
    const bool vanillaDepletes = GameInteractor_Should(VB_MAGIC_METER_DEPLETE_WHILE_CONSUMING, true);
    VmbCheck(vanillaDepletes, "vanilla control: stale infinite-magic flag suppressed depletion");
    Color_RGB8 vanillaFill = kVanillaGreen;
    const bool vanillaColorShould = GameInteractor_Should(VB_APPLY_MAGIC_METER_FILL_COLOR, true, &vanillaFill);
    VmbCheck(vanillaColorShould, "vanilla control: VB_APPLY_MAGIC_METER_FILL_COLOR altered should");
    VmbCheck(VmbSameRgb8(vanillaFill, kVanillaGreen), "vanilla control: stale infinite-magic flag recolored bar");

    randoCtx.GetOption(RSK_SHUFFLE_BOSS_SOULS).Set(RO_BOSS_SOULS_ON);
    Flags_UnsetRandomizerInf(RAND_INF_BARINADE_SOUL);
    const bool vanillaRematch = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, true, nullptr);
    const bool vanillaFirst = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, false, nullptr);
    VmbCheck(vanillaRematch, "vanilla control: VB_BARINADE_PLAY_REMATCH_INTRO suppressed rematch");
    VmbCheck(!vanillaFirst, "vanilla control: VB_BARINADE_PLAY_REMATCH_INTRO forced rematch on first fight");

    // (b) Randomizer file load: register RandomizerOnVanillaBehaviorHandler.
    const HOOK_ID vbHook = GameInteractor::Instance->RegisterGameHook<OnVb>(RandomizerOnVanillaBehaviorHandler);

    // Flag clear: magic depletes and fill stays green.
    Flags_UnsetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER);
    CVarSetInteger(kMagicInfChangedCvar, 0);
    const bool clearDepletes = GameInteractor_Should(VB_MAGIC_METER_DEPLETE_WHILE_CONSUMING, true);
    const bool clearPreservesFalse = !GameInteractor_Should(VB_MAGIC_METER_DEPLETE_WHILE_CONSUMING, false);
    VmbCheck(clearDepletes, "rando handler: magic did not deplete with infinite-magic flag clear");
    VmbCheck(clearPreservesFalse, "rando handler: VB_MAGIC_METER_DEPLETE_WHILE_CONSUMING ignored false input");
    Color_RGB8 randoClearFill = kVanillaGreen;
    GameInteractor_Should(VB_APPLY_MAGIC_METER_FILL_COLOR, true, &randoClearFill);
    VmbCheck(VmbSameRgb8(randoClearFill, kVanillaGreen), "rando handler: recolored magic bar with flag clear");

    // Flag set, cosmetic unchanged: magic does not deplete and fill becomes { 0, 0, 200 }.
    Flags_SetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER);
    const bool setDepletes = GameInteractor_Should(VB_MAGIC_METER_DEPLETE_WHILE_CONSUMING, true);
    VmbCheck(!setDepletes, "rando handler: magic still depleted with infinite-magic flag set");
    Color_RGB8 randoDefaultInfFill = kVanillaGreen;
    GameInteractor_Should(VB_APPLY_MAGIC_METER_FILL_COLOR, true, &randoDefaultInfFill);
    const bool isDefaultBlue = VmbSameRgb8(randoDefaultInfFill, kDefaultInfBlue);
    VmbCheck(isDefaultBlue, "rando handler: missing default { 0, 0, 200 } color");

    // Flag set, cosmetic customized: fill takes Consumable_MagicInfinite.Value.
    CVarSetInteger(kMagicInfChangedCvar, 1);
    CVarSetColor24(kMagicInfValueCvar, kCustomInfColor);
    Color_RGB8 randoCustomInfFill = kVanillaGreen;
    GameInteractor_Should(VB_APPLY_MAGIC_METER_FILL_COLOR, true, &randoCustomInfFill);
    const bool isCustomColor = VmbSameRgb8(randoCustomInfFill, kCustomInfColor);
    VmbCheck(isCustomColor, "rando handler: missing custom infinite-magic color");

    // Barinade rematch intro under boss-soul shuffle off vs on (missing vs obtained soul).
    randoCtx.GetOption(RSK_SHUFFLE_BOSS_SOULS).Set(RO_BOSS_SOULS_OFF);
    Flags_UnsetRandomizerInf(RAND_INF_BARINADE_SOUL);
    const bool soulsOffRematch = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, true, nullptr);
    const bool soulsOffFirst = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, false, nullptr);
    VmbCheck(soulsOffRematch, "rando handler: suppressed Barinade rematch while boss souls are Off");
    VmbCheck(!soulsOffFirst, "rando handler: played rematch intro on first fight while boss souls are Off");

    randoCtx.GetOption(RSK_SHUFFLE_BOSS_SOULS).Set(RO_BOSS_SOULS_ON);
    Flags_UnsetRandomizerInf(RAND_INF_BARINADE_SOUL);
    const bool missingSoulRematch = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, true, nullptr);
    const bool missingSoulFirst = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, false, nullptr);
    VmbCheck(!missingSoulRematch, "rando handler: played Barinade rematch intro without Barinade's soul");
    VmbCheck(!missingSoulFirst, "rando handler: played rematch intro on first fight without Barinade's soul");

    Flags_SetRandomizerInf(RAND_INF_BARINADE_SOUL);
    const bool foundSoulRematch = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, true, nullptr);
    const bool foundSoulFirst = GameInteractor_Should(VB_BARINADE_PLAY_REMATCH_INTRO, false, nullptr);
    VmbCheck(foundSoulRematch, "rando handler: suppressed Barinade rematch intro with Barinade's soul set");
    VmbCheck(!foundSoulFirst, "rando handler: played rematch intro on first fight with Barinade's soul set");

    // (c) Unregister + RemoveAllQueuedHooks (#7422): queued hook is removed immediately.
    GameInteractor::Instance->UnregisterGameHook<OnVb>(vbHook);
    const bool queuedBefore = !GameInteractor::HooksToUnregister<OnVb>::hooks.empty();
    VmbCheck(queuedBefore, "UnregisterGameHook did not enqueue vbHook in HooksToUnregister<OnVanillaBehavior>");
    GameInteractor::Instance->RemoveAllQueuedHooks();
    const bool drainedAfter = GameInteractor::HooksToUnregister<OnVb>::hooks.empty();
    const bool erasedAfter = GameInteractor::RegisteredGameHooks<OnVb>::functions.count(vbHook) == 0;
    VmbCheck(drainedAfter, "RemoveAllQueuedHooks did not drain HooksToUnregister<OnVanillaBehavior>");
    VmbCheck(erasedAfter, "RemoveAllQueuedHooks did not erase vbHook from RegisteredGameHooks");
    const bool detachedDepletes = GameInteractor_Should(VB_MAGIC_METER_DEPLETE_WHILE_CONSUMING, true);
    VmbCheck(detachedDepletes, "after RemoveAllQueuedHooks: detached hook still suppressed magic depletion");
}

} // namespace

extern "C" int OoT_VanillaMagicBoundary_RunHeadless(const char* sourceDir) {
    printf("[TEST] oot-vanilla-magic-boundary: infinite magic and Barinade rematch VB boundary (#1588)\n");
    sFailures = 0;

    VmbRunSourceLeg(sourceDir);

    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);

    SaveContext* savedSave = static_cast<SaveContext*>(malloc(sizeof(SaveContext)));
    if (savedSave == nullptr) {
        VmbFail("could not allocate SaveContext snapshot");
        Context_SetCurrentGame(prevGame);
        return 1;
    }
    memcpy(savedSave, &gSaveContext, sizeof(SaveContext));
    memset(&gSaveContext, 0, sizeof(SaveContext));

    const VmbSavedCVar savedChanged = VmbSaveCVar(kMagicInfChangedCvar);
    const VmbSavedCVar savedValue = VmbSaveCVar(kMagicInfValueCvar);

    std::shared_ptr<Rando::Context> prevRandoCtx = Rando::Context::GetInstance();
    std::shared_ptr<Rando::Context> createdRandoCtx;
    std::shared_ptr<Rando::Context> activeRandoCtx = prevRandoCtx;
    uint8_t prevBossSouls = RO_BOSS_SOULS_OFF;
    if (activeRandoCtx != nullptr) {
        prevBossSouls = activeRandoCtx->GetOption(RSK_SHUFFLE_BOSS_SOULS).Get();
    } else {
        createdRandoCtx = Rando::Context::CreateInstance();
        activeRandoCtx = createdRandoCtx;
    }

    if (activeRandoCtx == nullptr) {
        VmbFail("could not acquire Rando::Context instance");
    } else {
        VmbRunHandlerLeg(*activeRandoCtx);
    }

    if (createdRandoCtx != nullptr) {
        createdRandoCtx->GetLogic()->SetContext(nullptr);
        createdRandoCtx.reset();
    } else if (prevRandoCtx != nullptr) {
        prevRandoCtx->GetOption(RSK_SHUFFLE_BOSS_SOULS).Set(prevBossSouls);
    }

    VmbRestoreCVar(savedChanged);
    VmbRestoreCVar(savedValue);
    memcpy(&gSaveContext, savedSave, sizeof(SaveContext));
    free(savedSave);
    Context_SetCurrentGame(prevGame);

    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-vanilla-magic-boundary): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-vanilla-magic-boundary\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
