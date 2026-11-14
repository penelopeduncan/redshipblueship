/**
 * @file test_oot_mweep_stop.c
 * @brief ROM-free source regression check for #1591 and #1610 (`oot-mweep-stop-restores-camera`).
 *
 * Included at file scope by `src/common/test_runner.cpp` (compiled as C++).
 *
 * Upstream SoH #7349 (`2f9e96c7`) restores the camera if `Skip Story Cutscenes`
 * toggles on while King Zora is still mid-Mweep. In RSBS, `VB_PLAY_MWEEP_CS`
 * gates whether a cutscene camera was created in `EnKz_SetupMweep` rather than
 * using an intercept hook; both `EnKz_Mweep` (when `VB_PLAY_MWEEP_CS` turns
 * false mid-walk) and `EnKz_StopMweep` guard the camera and Link cutscene-action
 * restore behind `this->cutsceneCamera != CAM_ID_MAIN` and reset
 * `this->cutsceneCamera = CAM_ID_MAIN` so the subcamera is cleared at most once
 * and `CAM_ID_MAIN` is never hijacked or cleared.
 */

#include "../test_runner.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace {

static std::string OmsReadSource(const char* relPath) {
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

static bool OmsIsIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static size_t OmsSkipSpaceAndComments(const std::string& src, size_t pos) {
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

static std::string OmsExtractBraceBlock(const std::string& src, size_t openBracePos) {
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
static std::string OmsExtractFunctionBody(const std::string& src, const char* signature) {
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
static std::string OmsExtractIfBlock(const std::string& body, const char* conditionExpr, size_t* afterBlockOut) {
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
                const std::string block = OmsExtractBraceBlock(body, cur);
                if (!block.empty() && afterBlockOut != nullptr) {
                    *afterBlockOut = cur + block.size();
                }
                return block;
            }
        }
    }
    return std::string();
}

/** Return the `{ ... }` body when @p s starts (after whitespace/comments) with `if (<conditionExpr>) { ... }`. */
static std::string OmsExtractLeadingIfBlock(const std::string& s, const char* conditionExpr) {
    const size_t condLen = std::strlen(conditionExpr);
    const size_t pos = OmsSkipSpaceAndComments(s, 0);
    const bool nextOk = (pos + 2 >= s.size()) || !OmsIsIdentChar(s[pos + 2]);
    if (pos + 2 > s.size() || s.compare(pos, 2, "if") != 0 || !nextOk) {
        return std::string();
    }
    size_t cur = OmsSkipSpaceAndComments(s, pos + 2);
    if (cur >= s.size() || s[cur] != '(') {
        return std::string();
    }
    cur = OmsSkipSpaceAndComments(s, cur + 1);
    if (cur + condLen > s.size() || s.compare(cur, condLen, conditionExpr) != 0) {
        return std::string();
    }
    cur = OmsSkipSpaceAndComments(s, cur + condLen);
    if (cur >= s.size() || s[cur] != ')') {
        return std::string();
    }
    cur = OmsSkipSpaceAndComments(s, cur + 1);
    if (cur >= s.size() || s[cur] != '{') {
        return std::string();
    }
    return OmsExtractBraceBlock(s, cur);
}

/** Copy @p src with every comment replaced by one space and every string/char literal emptied. */
static std::string OmsStripCommentsAndStrings(const std::string& src) {
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
            pos = eol - 1; // the loop's pos++ lands on the newline, which is kept
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
            while (pos < src.size() && src[pos] != quote) {
                pos += (src[pos] == '\\') ? 2 : 1;
            }
            out.push_back(quote);
            continue;
        }
        out.push_back(c);
    }
    return out;
}

static size_t OmsCountSubstr(const std::string& haystack, const char* needle) {
    const size_t len = std::strlen(needle);
    if (len == 0) {
        return 0;
    }
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        count++;
        pos += len;
    }
    return count;
}

/**
 * True when @p block (a comment- and string-stripped `{ ... }`) runs every needle, in order, as a
 * statement of its own at the block's top level: brace depth 1 (not nested in an inner `{ }` such as
 * `if (0) { ... }`) and preceded only by whitespace after `{`, `;` or `}` (not the body of an
 * unbraced `if (0) call;`).
 */
static bool OmsHasTopLevelStatementsInOrder(const std::string& block, const char* const* needles, size_t count) {
    if (block.empty() || block[0] != '{') {
        return false;
    }
    size_t at = 0;
    for (size_t i = 0; i < count; i++) {
        bool matched = false;
        size_t found = at;
        while ((found = block.find(needles[i], found)) != std::string::npos) {
            int depth = 0;
            for (size_t k = 0; k < found; k++) {
                if (block[k] == '{') {
                    depth++;
                } else if (block[k] == '}') {
                    depth--;
                }
            }
            size_t prev = found;
            while (prev > 0 && (block[prev - 1] == ' ' || block[prev - 1] == '\t' || block[prev - 1] == '\n')) {
                prev--;
            }
            const char before = (prev > 0) ? block[prev - 1] : '\0';
            if (depth == 1 && (before == '{' || before == ';' || before == '}')) {
                matched = true;
                break;
            }
            found++;
        }
        if (!matched) {
            return false;
        }
        at = found + std::strlen(needles[i]);
    }
    return true;
}

/** Feed the matcher synthetic guard blocks it must reject, and one it must accept. */
static bool OmsMatcherSelfCheck(const char* const* needles, size_t count, const char** whyOut) {
    if (count != 4) {
        *whyOut = "self-check expects exactly four restore statements";
        return false;
    }
    const std::string a = needles[0];
    const std::string b = needles[1];
    const std::string c = needles[2];
    const std::string d = needles[3];
    const std::string good = "{\n    " + a + "\n    " + b + "\n    " + c + "\n    " + d + "\n}";
    if (!OmsHasTopLevelStatementsInOrder(OmsStripCommentsAndStrings(good), needles, count)) {
        *whyOut = "rejected a correct guard block";
        return false;
    }
    const std::string bad[] = {
        "{\n    " + a + "\n    // " + b + "\n    " + c + "\n    " + d + "\n}",
        "{\n    " + a + "\n    " + b + "\n    /* " + c + " */\n    " + d + "\n}",
        "{\n    " + a + "\n    if (0) {\n        " + b + "\n    }\n    " + c + "\n    " + d + "\n}",
        "{\n    " + a + "\n    " + b + "\n    if (0)\n        " + c + "\n    " + d + "\n}",
        "{\n    " + a + "\n    puts(\"" + b + "\");\n    " + c + "\n    " + d + "\n}",
        "{\n    " + a + "\n    " + c + "\n    " + b + "\n    " + d + "\n}",
    };
    for (const std::string& body : bad) {
        if (OmsHasTopLevelStatementsInOrder(OmsStripCommentsAndStrings(body), needles, count)) {
            *whyOut = "accepted a guard block whose restore call is commented out, dead-coded, quoted or misordered";
            return false;
        }
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

    static const char* const kRestoreCalls[] = {
        "OoT_Play_ChangeCameraStatus(play, this->gameplayCamera, CAM_STAT_ACTIVE);",
        "OoT_Play_ClearCamera(play, this->cutsceneCamera);",
        "this->cutsceneCamera = CAM_ID_MAIN;",
        "OoT_Player_SetCsActionWithHaltedActors(play, &this->actor, 7);",
    };
    const size_t kRestoreCallCount = sizeof(kRestoreCalls) / sizeof(kRestoreCalls[0]);
    const char* why = nullptr;
    if (!OmsMatcherSelfCheck(kRestoreCalls, kRestoreCallCount, &why)) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): matcher self-check: %s\n", why);
        return TEST_FAIL;
    }

    const std::string stopBody = OmsExtractFunctionBody(src, "void EnKz_StopMweep(EnKz* this, PlayState* play)");
    if (stopBody.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_StopMweep not found\n");
        return TEST_FAIL;
    }
    const std::string stopCode = OmsStripCommentsAndStrings(stopBody);
    if (stopCode.find("VB_PLAY_MWEEP_CS") != std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_StopMweep still checks VB_PLAY_MWEEP_CS\n");
        return TEST_FAIL;
    }

    const std::string guardBlock = OmsExtractIfBlock(stopBody, "this->cutsceneCamera != CAM_ID_MAIN", nullptr);
    if (guardBlock.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): missing cutsceneCamera != CAM_ID_MAIN guard\n");
        return TEST_FAIL;
    }
    if (!OmsHasTopLevelStatementsInOrder(OmsStripCommentsAndStrings(guardBlock), kRestoreCalls, kRestoreCallCount)) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): restore calls missing or misordered\n");
        return TEST_FAIL;
    }
    if (stopCode.find("this->actionFunc = EnKz_Wait;") == std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_StopMweep missing actionFunc reset\n");
        return TEST_FAIL;
    }

    const std::string mweepBody = OmsExtractFunctionBody(src, "void EnKz_Mweep(EnKz* this, PlayState* play)");
    if (mweepBody.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_Mweep not found\n");
        return TEST_FAIL;
    }
    const std::string mweepCode = OmsStripCommentsAndStrings(mweepBody);
    size_t afterPlayIf = 0;
    const std::string playCsBlock =
        OmsExtractIfBlock(mweepBody, "GameInteractor_Should(VB_PLAY_MWEEP_CS, true)", &afterPlayIf);
    if (playCsBlock.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_Mweep missing VB_PLAY_MWEEP_CS branch\n");
        return TEST_FAIL;
    }
    const std::string setEyeGuard = OmsExtractIfBlock(playCsBlock, "this->cutsceneCamera != CAM_ID_MAIN", nullptr);
    static const char* const kSetEyeCalls[] = {
        "Play_CameraSetAtEye(play, this->cutsceneCamera, &pos, &initPos);",
    };
    const std::string setEyeCode = OmsStripCommentsAndStrings(setEyeGuard);
    if (setEyeGuard.empty() || !OmsHasTopLevelStatementsInOrder(setEyeCode, kSetEyeCalls, 1)) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_Mweep Play_CameraSetAtEye guard missing\n");
        return TEST_FAIL;
    }
    if (OmsCountSubstr(mweepCode, "Play_CameraSetAtEye") != 1) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_Mweep has unguarded Play_CameraSetAtEye\n");
        return TEST_FAIL;
    }
    const size_t elsePos = OmsSkipSpaceAndComments(mweepBody, afterPlayIf);
    const bool elseNextOk = (elsePos + 4 >= mweepBody.size()) || !OmsIsIdentChar(mweepBody[elsePos + 4]);
    if (elsePos + 4 > mweepBody.size() || mweepBody.compare(elsePos, 4, "else") != 0 || !elseNextOk) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_Mweep missing mid-walk else branch\n");
        return TEST_FAIL;
    }
    const std::string midGuard =
        OmsExtractLeadingIfBlock(mweepBody.substr(elsePos + 4), "this->cutsceneCamera != CAM_ID_MAIN");
    const std::string midCode = OmsStripCommentsAndStrings(midGuard);
    if (midGuard.empty() || !OmsHasTopLevelStatementsInOrder(midCode, kRestoreCalls, kRestoreCallCount)) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_Mweep mid-walk camera restore missing\n");
        return TEST_FAIL;
    }
    if (OmsCountSubstr(mweepCode, "OoT_Play_ClearCamera") != 1) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_Mweep has extra OoT_Play_ClearCamera\n");
        return TEST_FAIL;
    }

    const std::string setupBody = OmsExtractFunctionBody(src, "void EnKz_SetupMweep(EnKz* this, PlayState* play)");
    if (setupBody.empty()) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_SetupMweep not found\n");
        return TEST_FAIL;
    }
    const std::string setupCode = OmsStripCommentsAndStrings(setupBody);
    if (setupCode.find("VB_PLAY_MWEEP_CS") == std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_SetupMweep lost VB_PLAY_MWEEP_CS\n");
        return TEST_FAIL;
    }
    if (setupCode.find("OoT_Play_CreateSubCamera(play)") == std::string::npos) {
        printf("[TEST] FAIL (oot-mweep-stop-restores-camera): EnKz_SetupMweep missing CreateSubCamera\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: oot-mweep-stop-restores-camera\n");
    return TEST_PASS;
#endif
}
