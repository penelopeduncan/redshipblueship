/**
 * @file test_extractor_picker_guard.c
 * @brief Source lock for the non-Windows file-picker availability guard in both
 *        extractors and the OoT extraction-failure popup wiring (#1590).
 *
 * ROM-free, display-free (default redship tier). Included at FILE SCOPE by
 * src/common/test_runner.cpp (compiled as C++).
 */

#include "../test_runner.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>

#ifndef RSBS_SOURCE_DIR

static TestResult Test_ExtractorPickerGuard(void) {
    printf("[TEST] extractor-picker-guard: Linux file-picker guard (OoT/MM) and OoT extraction-failure popup "
           "(#1590)\n");
    printf("[TEST] FAIL: RSBS_SOURCE_DIR is undefined, so ExtractorPickerGuard cannot read source files\n");
    return TEST_FAIL;
}

#else

namespace {

int sEpgFailures = 0;

#define EPG_CHECK(cond, ...)                                       \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (extractor-picker-guard): ");      \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sEpgFailures++;                                        \
        }                                                          \
    } while (0)

std::string EpgReadSource(const char* rel) {
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c != '\r') {
            out.push_back(c);
        }
    }
    return out;
}

// Skips past a string/char literal or C/C++ comment starting at index i.
// Returns true and advances i if one started at i; otherwise returns false.
bool EpgSkipLiteralOrComment(const std::string& src, size_t& i, bool skipStrings) {
    const char c = src[i];
    if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
        i += 2;
        while (i < src.size() && src[i] != '\n') {
            i++;
        }
        return true;
    }
    if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
        i += 2;
        while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) {
            i++;
        }
        i = (i + 1 < src.size()) ? i + 2 : src.size();
        return true;
    }
    if (skipStrings && (c == '"' || c == '\'')) {
        const char quote = c;
        i++;
        while (i < src.size()) {
            if (src[i] == '\\' && i + 1 < src.size()) {
                i += 2;
                continue;
            }
            if (src[i] == quote) {
                i++;
                break;
            }
            i++;
        }
        return true;
    }
    return false;
}

std::string EpgMatchBraceBlock(const std::string& src, size_t openBracePos) {
    if (openBracePos >= src.size() || src[openBracePos] != '{') {
        return std::string();
    }
    int depth = 0;
    size_t i = openBracePos;
    while (i < src.size()) {
        if (EpgSkipLiteralOrComment(src, i, true)) {
            continue;
        }
        if (src[i] == '{') {
            depth++;
        } else if (src[i] == '}') {
            depth--;
            if (depth == 0) {
                return src.substr(openBracePos, i - openBracePos + 1);
            }
        }
        i++;
    }
    return std::string();
}

std::string EpgFunctionBody(const std::string& src, const char* signature) {
    size_t at = 0;
    const size_t sigLen = strlen(signature);
    while ((at = src.find(signature, at)) != std::string::npos) {
        size_t p = at + sigLen;
        while (p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\t')) {
            p++;
        }
        if (p < src.size() && src[p] == '{') {
            return EpgMatchBraceBlock(src, p);
        }
        at = p;
    }
    return std::string();
}

// Strips comments and collapses outside-literal whitespace runs to single spaces.
std::string EpgNormalizeCode(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool pendingSpace = false;
    size_t i = 0;
    while (i < src.size()) {
        const char c = src[i];
        if (c == '/' && i + 1 < src.size() && (src[i + 1] == '/' || src[i + 1] == '*')) {
            EpgSkipLiteralOrComment(src, i, false);
            pendingSpace = !out.empty();
            continue;
        }
        if (c == '"' || c == '\'') {
            if (pendingSpace) {
                out.push_back(' ');
                pendingSpace = false;
            }
            const size_t start = i;
            EpgSkipLiteralOrComment(src, i, true);
            out.append(src, start, i - start);
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!out.empty()) {
                pendingSpace = true;
            }
            i++;
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        out.push_back(c);
        i++;
    }
    return out;
}

size_t EpgCountOccurrences(const std::string& hay, const char* needle) {
    const size_t len = strlen(needle);
    if (len == 0) {
        return 0;
    }
    size_t count = 0;
    size_t at = 0;
    while ((at = hay.find(needle, at)) != std::string::npos) {
        count++;
        at += len;
    }
    return count;
}

bool EpgInOrder(const std::string& hay, std::initializer_list<const char*> needles) {
    size_t at = 0;
    for (const char* n : needles) {
        const size_t f = hay.find(n, at);
        if (f == std::string::npos) {
            return false;
        }
        at = f + strlen(n);
    }
    return true;
}

void EpgCheckPickerGuard(const char* relPath, const char* signature) {
    const std::string src = EpgReadSource(relPath);
    EPG_CHECK(!src.empty(), "could not read %s\n", relPath);
    if (src.empty()) {
        return;
    }
    const std::string body = EpgFunctionBody(src, signature);
    EPG_CHECK(!body.empty(), "%s: missing %s\n", relPath, signature);
    if (body.empty()) {
        return;
    }

    const size_t elsePos = body.find("\n#else");
    EPG_CHECK(elsePos != std::string::npos, "%s: missing #else branch in %s\n", relPath, signature);
    if (elsePos == std::string::npos) {
        return;
    }
    const size_t endifPos = body.find("\n#endif", elsePos);
    EPG_CHECK(endifPos != std::string::npos, "%s: missing #endif after #else in %s\n", relPath, signature);
    if (endifPos == std::string::npos) {
        return;
    }

    const std::string elseNorm = EpgNormalizeCode(body.substr(elsePos, endifPos - elsePos));
    const size_t openPos = elseNorm.find("pfd::open_file(");
    EPG_CHECK(openPos != std::string::npos, "%s: missing pfd::open_file( in #else branch\n", relPath);
    if (openPos == std::string::npos) {
        return;
    }

    const std::string beforeOpen = elseNorm.substr(0, openPos);
    const char* guardHead = "if (!pfd::settings::available())";
    const size_t guardPos = beforeOpen.find(guardHead);
    EPG_CHECK(guardPos != std::string::npos,
              "%s: missing if (!pfd::settings::available()) before pfd::open_file( in #else branch\n", relPath);
    if (guardPos == std::string::npos) {
        return;
    }

    const size_t bracePos = beforeOpen.find('{', guardPos + strlen(guardHead));
    const std::string guardBlock =
        bracePos == std::string::npos ? std::string() : EpgMatchBraceBlock(beforeOpen, bracePos);
    EPG_CHECK(!guardBlock.empty(), "%s: malformed if (!pfd::settings::available()) block\n", relPath);
    EPG_CHECK(guardBlock.find("ShowErrorBox(\"File Picker Unavailable\"") != std::string::npos,
              "%s: picker guard does not call ShowErrorBox(\"File Picker Unavailable\"\n", relPath);
    EPG_CHECK(guardBlock.find("RedShipBlueShip needs one of these to select a ROM") != std::string::npos,
              "%s: picker guard message does not carry RedShipBlueShip prompt text\n", relPath);
    EPG_CHECK(guardBlock.find("return false;") != std::string::npos, "%s: picker guard does not return false\n",
              relPath);
}

void EpgCheckOotExtractFailureWiring(void) {
    const char* relPath = "games/oot/soh/OTRGlobals.cpp";
    const std::string src = EpgReadSource(relPath);
    EPG_CHECK(!src.empty(), "could not read %s\n", relPath);
    if (src.empty()) {
        return;
    }
    const std::string runExtract = EpgFunctionBody(src, "void OTRGlobals::RunExtract(int argc, char* argv[])");
    EPG_CHECK(!runExtract.empty(), "%s: missing OTRGlobals::RunExtract\n", relPath);
    if (runExtract.empty()) {
        return;
    }

    const std::string norm = EpgNormalizeCode(runExtract);
    const size_t totalCalls = EpgCountOccurrences(norm, "extract.CallZapd(");
    const size_t wiredCalls = EpgCountOccurrences(norm, "extractFailed = !extract.CallZapd(");
    EPG_CHECK(totalCalls == 4, "%s: expected 4 extract.CallZapd( calls in RunExtract, found %zu\n", relPath,
              totalCalls);
    EPG_CHECK(wiredCalls == 4, "%s: expected 4 extractFailed = !extract.CallZapd( sites in RunExtract, found %zu\n",
              relPath, wiredCalls);

    // PS_SECOND keeps asking until the player says No (upstream 5c6006ee): a
    // cancelled or failed pick loops back to the popup, so the only moves to
    // ES_VERIFY are a successful second rom (in the task) and the No button.
    const char* secondHead = "case PS_SECOND: {";
    const size_t secondPos = norm.find(secondHead);
    EPG_CHECK(secondPos != std::string::npos, "%s: missing case PS_SECOND in RunExtract\n", relPath);
    if (secondPos != std::string::npos) {
        const std::string secondBlock = EpgMatchBraceBlock(norm, secondPos + strlen(secondHead) - 1);
        const size_t verifyMoves = EpgCountOccurrences(secondBlock, "extractStep = ES_VERIFY;");
        EPG_CHECK(verifyMoves == 2,
                  "%s: PS_SECOND must move to ES_VERIFY only after a good second rom and on No, found %zu moves\n",
                  relPath, verifyMoves);
        EPG_CHECK(EpgInOrder(secondBlock, { "if (!extractFailed && generatedFirst) {", "extractStep = ES_VERIFY;",
                                            "[&]() { extractStep = ES_VERIFY; }" }),
                  "%s: PS_SECOND ES_VERIFY moves are not the good-second-rom and No paths\n", relPath);
    }

    const char* popupGate = "if (extractFailed)";
    const size_t getPos = norm.find("extractionTask->get();");
    EPG_CHECK(getPos != std::string::npos, "%s: missing extractionTask->get(); in RunExtract\n", relPath);
    const size_t gatePos = getPos == std::string::npos ? std::string::npos : norm.find(popupGate, getPos);
    EPG_CHECK(gatePos != std::string::npos, "%s: missing if (extractFailed) popup gate after extractionTask->get()\n",
              relPath);
    EPG_CHECK(
        EpgInOrder(norm, { "extractionTask->get();", "lastExtractFailed = extractFailed;", "if (extractFailed)" }),
        "%s: lastExtractFailed must be latched from extractFailed before if (extractFailed)\n", relPath);
    if (gatePos != std::string::npos) {
        const size_t bracePos = norm.find('{', gatePos + strlen(popupGate));
        const std::string gateBlock =
            bracePos == std::string::npos ? std::string() : EpgMatchBraceBlock(norm, bracePos);
        EPG_CHECK(gateBlock.find("SohGui::RegisterPopup(\"Extraction Failed\"") != std::string::npos,
                  "%s: if (extractFailed) block does not register \"Extraction Failed\" popup\n", relPath);
        EPG_CHECK(gateBlock.find("extractFailed = false;") != std::string::npos,
                  "%s: if (extractFailed) block does not clear extractFailed\n", relPath);
    }
}

} // namespace

static TestResult Test_ExtractorPickerGuard(void) {
    printf("[TEST] extractor-picker-guard: Linux file-picker guard (OoT/MM) and OoT extraction-failure popup "
           "(#1590)\n");
    sEpgFailures = 0;
    EpgCheckPickerGuard("games/oot/soh/Extractor/Extract.cpp", "bool Extractor::GetRomPathFromBox()");
    EpgCheckPickerGuard("games/mm/2s2h/Extractor/Extract.cpp", "bool MMExtractor::GetRomPathFromBox()");
    EpgCheckOotExtractFailureWiring();
    if (sEpgFailures != 0) {
        printf("[TEST] FAIL: extractor-picker-guard (%d failure(s))\n", sEpgFailures);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: extractor-picker-guard\n");
    return TEST_PASS;
}

#endif
