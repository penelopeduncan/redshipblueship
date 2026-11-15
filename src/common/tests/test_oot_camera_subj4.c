/**
 * @file test_oot_camera_subj4.c
 * @brief ROM-free source regression check for SoH #6709 (`oot-camera-subj4-console`).
 *
 * Included at file scope by `src/common/test_runner.cpp` (compiled as C++).
 *
 * Upstream SoH #6709 (`3e1318edc`) uncomments three statements in
 * `Camera_Subj4` (the crawlspace camera) that the decompilation had left as
 * comments. On console the camera writes Link's world position from its own
 * eye position, puts him on the ground under it and turns his shape yaw to the
 * camera's; without those writes the crawlspace glitch behaves differently
 * from console. Camera_Subj4 needs a live PlayState with collision, so this
 * row reads the function from source: the three statements must be live code
 * (not commented out, quoted or dead-coded), at the top level of the function
 * body, in upstream's order, between `anim->unk_28 = temp_f16;` and the
 * `240.0f` scale that follows it.
 */

#include "../test_runner.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string OcsReadSource(const char* relPath) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + relPath, std::ios::binary);
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
#else
    (void)relPath;
    return std::string();
#endif
}

/** @p src with comments and string/char literals blanked to spaces (newlines kept). */
std::string OcsStripCommentsAndStrings(const std::string& src) {
    std::string out = src;
    size_t i = 0;
    while (i < out.size()) {
        const char c = out[i];
        if (c == '/' && i + 1 < out.size() && out[i + 1] == '/') {
            while (i < out.size() && out[i] != '\n') {
                out[i++] = ' ';
            }
        } else if (c == '/' && i + 1 < out.size() && out[i + 1] == '*') {
            out[i++] = ' ';
            out[i++] = ' ';
            while (i < out.size() && !(out[i] == '*' && i + 1 < out.size() && out[i + 1] == '/')) {
                if (out[i] != '\n') {
                    out[i] = ' ';
                }
                i++;
            }
            if (i < out.size()) {
                out[i++] = ' ';
                out[i++] = ' ';
            }
        } else if (c == '"' || c == '\'') {
            const char quote = c;
            out[i++] = ' ';
            while (i < out.size() && out[i] != quote) {
                if (out[i] == '\\' && i + 1 < out.size()) {
                    out[i++] = ' ';
                }
                if (out[i] != '\n') {
                    out[i] = ' ';
                }
                i++;
            }
            if (i < out.size()) {
                out[i++] = ' ';
            }
        } else {
            i++;
        }
    }
    return out;
}

/** The braced body of the definition starting with @p signature, or "". */
std::string OcsFunctionBody(const std::string& code, const char* signature) {
    size_t at = 0;
    while ((at = code.find(signature, at)) != std::string::npos) {
        size_t p = at + strlen(signature);
        while (p < code.size() && (code[p] == ' ' || code[p] == '\n' || code[p] == '\t')) {
            p++;
        }
        if (p < code.size() && code[p] == '{') {
            int depth = 0;
            for (size_t q = p; q < code.size(); q++) {
                if (code[q] == '{') {
                    depth++;
                } else if (code[q] == '}') {
                    depth--;
                    if (depth == 0) {
                        return code.substr(p, q - p + 1);
                    }
                }
            }
            return std::string();
        }
        at = p;
    }
    return std::string();
}

/** @p head begins with the keyword @p kw followed by a space or '('. */
bool OcsStartsWithKeyword(const std::string& head, const char* kw) {
    const size_t n = strlen(kw);
    return head.compare(0, n, kw) == 0 && head.size() > n && (head[n] == ' ' || head[n] == '(');
}

/** The top-level statements of a braced, comment-free @p body, whitespace-collapsed.
 *  A statement nested in braces or under an unbraced if/else/for/while is not top level. */
bool OcsTopLevelStatementsInOrder(const std::string& body, const char* const* needles, size_t count) {
    if (body.size() < 2 || body[0] != '{') {
        return false;
    }
    std::string stmt;
    int depth = 0;
    int parens = 0;
    size_t next = 0;
    bool controlled = false; // the previous top-level token was an unbraced control head
    for (size_t i = 1; i + 1 < body.size() && next < count; i++) {
        const char c = body[i];
        if (c == '{') {
            depth++;
            stmt.clear();
            controlled = false;
            continue;
        }
        if (c == '}') {
            depth--;
            stmt.clear();
            continue;
        }
        if (depth != 0) {
            continue;
        }
        if (c == '(') {
            parens++;
        } else if (c == ')') {
            parens--;
        }
        if (c == ' ' || c == '\t' || c == '\n') {
            if (!stmt.empty() && stmt.back() != ' ') {
                stmt.push_back(' ');
            }
            continue;
        }
        stmt.push_back(c);
        if (c == ')' && parens == 0) {
            std::string head = stmt;
            while (!head.empty() && head[0] == ' ') {
                head.erase(0, 1);
            }
            if (OcsStartsWithKeyword(head, "if") || OcsStartsWithKeyword(head, "for") ||
                OcsStartsWithKeyword(head, "while") || OcsStartsWithKeyword(head, "else if")) {
                controlled = true;
                stmt.clear();
                continue;
            }
        }
        if (c == ';' && parens == 0) {
            while (!stmt.empty() && stmt[0] == ' ') {
                stmt.erase(0, 1);
            }
            if (stmt == "else" || stmt.rfind("else ", 0) == 0) {
                controlled = false;
                stmt.clear();
                continue;
            }
            if (!controlled && stmt == needles[next]) {
                next++;
            } else {
                // The needles must be consecutive statements: start over, and let
                // this statement begin the run if it is the first needle.
                next = (!controlled && stmt == needles[0]) ? 1 : 0;
            }
            controlled = false;
            stmt.clear();
        }
    }
    return next == count;
}

const char* const kOcsConsoleWrites[] = {
    "anim->unk_28 = temp_f16;",
    "camera->player->actor.world.pos = *eyeNext;",
    "camera->player->actor.world.pos.y = camera->playerGroundY;",
    "camera->player->actor.shape.rot.y = sp64.yaw;",
    "temp_f16 = ((240.0f * temp_f16) * (anim->unk_24 * 0.416667f));",
};
const size_t kOcsConsoleWriteCount = sizeof(kOcsConsoleWrites) / sizeof(kOcsConsoleWrites[0]);

/** The matcher accepts the live block and rejects each way it could be disabled. */
bool OcsMatcherSelfCheck(const char** whyOut) {
    std::string good = "{\n";
    for (size_t i = 0; i < kOcsConsoleWriteCount; i++) {
        good += std::string("    ") + kOcsConsoleWrites[i] + "\n";
    }
    good += "}";
    if (!OcsTopLevelStatementsInOrder(OcsStripCommentsAndStrings(good), kOcsConsoleWrites, kOcsConsoleWriteCount)) {
        *whyOut = "rejected the live block";
        return false;
    }
    const std::string a = kOcsConsoleWrites[0];
    const std::string b = kOcsConsoleWrites[2];
    const std::string tail = std::string(kOcsConsoleWrites[3]) + "\n    " + kOcsConsoleWrites[4];
    const std::string w = kOcsConsoleWrites[1];
    const std::string bad[] = {
        "{\n    " + a + "\n    // " + w + "\n    " + b + "\n    " + tail + "\n}",
        "{\n    " + a + "\n    /* " + w + " */\n    " + b + "\n    " + tail + "\n}",
        "{\n    " + a + "\n    if (0) {\n        " + w + "\n    }\n    " + b + "\n    " + tail + "\n}",
        "{\n    " + a + "\n    if (0)\n        " + w + "\n    " + b + "\n    " + tail + "\n}",
        "{\n    " + a + "\n    puts(\"" + w + "\");\n    " + b + "\n    " + tail + "\n}",
        "{\n    " + a + "\n    " + b + "\n    " + w + "\n    " + tail + "\n}",
    };
    for (const std::string& body : bad) {
        if (OcsTopLevelStatementsInOrder(OcsStripCommentsAndStrings(body), kOcsConsoleWrites, kOcsConsoleWriteCount)) {
            *whyOut = "accepted a block whose write is commented out, dead-coded, quoted or misordered";
            return false;
        }
    }
    return true;
}

} // namespace

TestResult Test_OoTCameraSubj4Console(void) {
#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL (oot-camera-subj4-console): RSBS_SOURCE_DIR is undefined\n");
    return TEST_FAIL;
#else
    const char* why = nullptr;
    if (!OcsMatcherSelfCheck(&why)) {
        printf("[TEST] FAIL (oot-camera-subj4-console): matcher self-check: %s\n", why);
        return TEST_FAIL;
    }
    const std::string src = OcsReadSource("games/oot/src/code/z_camera.c");
    if (src.empty()) {
        printf("[TEST] FAIL (oot-camera-subj4-console): cannot read games/oot/src/code/z_camera.c\n");
        return TEST_FAIL;
    }
    const std::string body = OcsFunctionBody(OcsStripCommentsAndStrings(src), "s32 Camera_Subj4(Camera* camera)");
    if (body.empty()) {
        printf("[TEST] FAIL (oot-camera-subj4-console): Camera_Subj4 not found\n");
        return TEST_FAIL;
    }
    if (!OcsTopLevelStatementsInOrder(body, kOcsConsoleWrites, kOcsConsoleWriteCount)) {
        printf("[TEST] FAIL (oot-camera-subj4-console): Camera_Subj4 does not write Link's position and yaw from "
               "the crawlspace camera as console does (SoH #6709)\n");
        return TEST_FAIL;
    }
    printf("[TEST] PASS: oot-camera-subj4-console\n");
    return TEST_PASS;
#endif
}
