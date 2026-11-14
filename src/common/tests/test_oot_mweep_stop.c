/**
 * @file test_oot_mweep_stop.c
 * @brief ROM-free source regression check for #1591 (`oot-mweep-stop-restores-camera`).
 *
 * Included at file scope by `src/common/test_runner.cpp` (compiled as C++).
 *
 * Upstream SoH #7349 (`2f9e96c7`) restores the camera if `Skip Story Cutscenes`
 * toggles on while King Zora is still mid-Mweep. In RSBS, `VB_PLAY_MWEEP_CS`
 * gates whether a cutscene camera was created in `EnKz_SetupMweep` rather than
 * using an intercept hook; `EnKz_StopMweep` therefore must guard the camera
 * and Link cutscene-action restore behind `this->cutsceneCamera != CAM_ID_MAIN`
 * instead of re-querying `GameInteractor_Should(VB_PLAY_MWEEP_CS, true)`.
 */

#include "../test_runner.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string OmsReadSource(const char* relPath) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + relPath, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // Normalize CRLF -> LF so needles and brace scanners behave identically on Windows.
    std::string out;
    out.reserve(raw.size());
    for (const char ch : raw) {
        if (ch != '\r') {
            out.push_back(ch);
        }
    }
    return out;
#else
    (void)relPath;
    return std::string();
#endif
}

bool OmsIsIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

size_t OmsSkipSpaceAndComments(const std::string& src, size_t pos) {
    while (pos < src.size()) {
        const char c = src[pos];
        if (c == ' ' || c == '\t' || c == '\n') {
            pos++;
            continue;
        }
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            const size_t eol = src.find('\n', pos + 2);
            pos = (eol == std::string::npos) ? src.size() : (eol + 1);
            continue;
        }
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '*') {
            const size_t end = src.find("*/", pos + 2);
            pos = (end == std::string::npos) ? src.size() : (end + 2);
            continue;
        }
        break;
    }
    return pos;
}

std::string OmsExtractBraceBlock(const std::string& src, size_t openBracePos) {
    if (openBracePos >= src.size() || src[openBracePos] != '{') {
        return std::string();
    }
    int depth = 0;
    for (size_t pos = openBracePos; pos < src.size(); pos++) {
        const char c = src[pos];
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            const size_t eol = src.find('\n', pos + 2);
            if (eol == std::string::npos) {
                return std::string();
            }
            pos = eol;
            continue;
        }
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '*') {
            const size_t end = src.find("*/", pos + 2);
            if (end == std::string::npos) {
                return std::string();
            }
            pos = end + 1;
            continue;
        }
        if (c == '"' || c == '\'') {
            const char quote = c;
            pos++;
            while (pos < src.size()) {
                if (src[pos] == '\\') {
                    pos += 2;
                    continue;
                }
                if (src[pos] == quote) {
                    break;
                }
                pos++;
            }
            if (pos >= src.size()) {
                return std::string();
            }
            continue;
        }
        if (c == '{') {
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                return src.substr(openBracePos, pos - openBracePos + 1);
            }
        }
    }
    return std::string();
}

/** Return the `{ ... }` body of the definition starting with @p signature (skipping prototypes). */
std::string OmsExtractFunctionBody(const std::string& src, const char* signature) {
    const size_t sigLen = std::strlen(signature);
    size_t at = 0;
    while ((at = src.find(signature, at)) != std::string::npos) {
        if (at > 0 && src[at - 1] != '\n') {
            at += sigLen;
            continue;
        }
        const size_t openBrace = OmsSkipSpaceAndComments(src, at + sigLen);
        if (openBrace < src.size() && src[openBrace] == '{') {
            return OmsExtractBraceBlock(src, openBrace);
        }
        at = openBrace;
    }
    return std::string();
}

/** Return the `{ ... }` body of an `if (<conditionExpr>) { ... }` statement outside comments/strings. */
std::string OmsExtractIfBlock(const std::string& body, const char* conditionExpr) {
    const size_t condLen = std::strlen(conditionExpr);
    for (size_t pos = 0; pos < body.size(); pos++) {
        const char c = body[pos];
        if (c == '/' && pos + 1 < body.size() && body[pos + 1] == '/') {
            const size_t eol = body.find('\n', pos + 2);
            if (eol == std::string::npos) {
                break;
            }
            pos = eol;
            continue;
        }
        if (c == '/' && pos + 1 < body.size() && body[pos + 1] == '*') {
            const size_t end = body.find("*/", pos + 2);
            if (end == std::string::npos) {
                break;
            }
            pos = end + 1;
            continue;
        }
        if (c == '"' || c == '\'') {
            const char quote = c;
            pos++;
            while (pos < body.size()) {
                if (body[pos] == '\\') {
                    pos += 2;
                    continue;
                }
                if (body[pos] == quote) {
                    break;
                }
                pos++;
            }
            if (pos >= body.size()) {
                break;
            }
            continue;
        }
        if (pos + 2 <= body.size() && body.compare(pos, 2, "if") == 0) {
            const bool prevOk = (pos == 0) || !OmsIsIdentChar(body[pos - 1]);
            const bool nextOk = (pos + 2 >= body.size()) || !OmsIsIdentChar(body[pos + 2]);
            if (!prevOk || !nextOk) {
                continue;
            }
            size_t cur = OmsSkipSpaceAndComments(body, pos + 2);
            if (cur >= body.size() || body[cur] != '(') {
                continue;
            }
            cur = OmsSkipSpaceAndComments(body, cur + 1);
            if (cur + condLen > body.size() || body.compare(cur, condLen, conditionExpr) != 0) {
                continue;
            }
            cur = OmsSkipSpaceAndComments(body, cur + condLen);
            if (cur >= body.size() || body[cur] != ')') {
                continue;
            }
            cur = OmsSkipSpaceAndComments(body, cur + 1);
            if (cur < body.size() && body[cur] == '{') {
                return OmsExtractBraceBlock(body, cur);
            }
        }
    }
    return std::string();
}

bool OmsContainsInOrder(const std::string& hay, const char* const* needles, size_t count) {
    if (hay.empty()) {
        return false;
    }
    size_t at = 0;
    for (size_t i = 0; i < count; i++) {
        const size_t found = hay.find(needles[i], at);
        if (found == std::string::npos) {
            return false;
        }
        at = found + std::strlen(needles[i]);
    }
    return true;
}

} // namespace

TestResult Test_OoTMweepStopRestoresCamera(void) {
#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL (oot-mweep-stop-restores-camera): RSBS_SOURCE_DIR is undefined\n");
    return TEST_FAIL;
#else
    const std::string src = OmsReadSource("games/oot/src/overlays/actors/ovl_En_Kz/z_en_kz.c");
    if (src.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): cannot read z_en_kz.c\n");
        return TEST_FAIL;
    }

    const std::string stopBody = OmsExtractFunctionBody(src, "void EnKz_StopMweep(EnKz* this, PlayState* play)");
    if (stopBody.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_StopMweep not found\n");
        return TEST_FAIL;
    }
    if (stopBody.find("VB_PLAY_MWEEP_CS") != std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_StopMweep still checks VB_PLAY_MWEEP_CS\n");
        return TEST_FAIL;
    }

    const std::string guardBlock = OmsExtractIfBlock(stopBody, "this->cutsceneCamera != CAM_ID_MAIN");
    if (guardBlock.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): missing cutsceneCamera != CAM_ID_MAIN guard\n");
        return TEST_FAIL;
    }

    static const char* const kRestoreCalls[] = {
        "OoT_Play_ChangeCameraStatus(play, this->gameplayCamera, CAM_STAT_ACTIVE);",
        "OoT_Play_ClearCamera(play, this->cutsceneCamera);",
        "OoT_Player_SetCsActionWithHaltedActors(play, &this->actor, 7);",
    };
    const size_t kRestoreCallCount = sizeof(kRestoreCalls) / sizeof(kRestoreCalls[0]);
    if (!OmsContainsInOrder(guardBlock, kRestoreCalls, kRestoreCallCount)) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): restore calls missing or misordered\n");
        return TEST_FAIL;
    }
    if (stopBody.find("this->actionFunc = EnKz_Wait;") == std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_StopMweep missing actionFunc reset\n");
        return TEST_FAIL;
    }

    const std::string setupBody = OmsExtractFunctionBody(src, "void EnKz_SetupMweep(EnKz* this, PlayState* play)");
    if (setupBody.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_SetupMweep not found\n");
        return TEST_FAIL;
    }
    if (setupBody.find("VB_PLAY_MWEEP_CS") == std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_SetupMweep lost VB_PLAY_MWEEP_CS\n");
        return TEST_FAIL;
    }
    if (setupBody.find("OoT_Play_CreateSubCamera(play)") == std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_SetupMweep missing CreateSubCamera\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: oot-mweep-stop-restores-camera\n");
    return TEST_PASS;
#endif
}
