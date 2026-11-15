/**
 * @file test_oot_check_receive_effects.c
 * @brief OoT's CHECK-KEYED RECEIVE EFFECTS RUN FOR A FOREIGN HOST TOO (#1602,
 *        #1600 step 2).
 *
 * ============================================================================
 * oot-check-receive-effects (CTest OoTCheckReceiveEffects, redship tier)
 * ============================================================================
 *
 * RandomizerOnItemReceiveHandler (hook_handlers.cpp) is reached only by a
 * NATIVE item. What it does because of WHICH CHECK gave the item (not which
 * item) must also happen when that check hosts a Majora's Mask item, which no
 * native receive follows. That part lives in RandomizerOnCheckReceived, called
 * from the receive handler, from the show-only give point's release and from the
 * drain's foreign branch when no cutscene shows the item.
 *
 *  (1) THE LITERAL. kCheckKeyed below names every check the receive path tests
 *      today. From source (RSBS_SOURCE_DIR): the checks RandomizerOnCheckReceived
 *      compares `rc` with are exactly kCheckKeyed, and the receive handler itself
 *      compares no check (no `GetRandomizerCheck() ==`, no `== RC_*` other than
 *      the free slot's RC_UNKNOWN_CHECK). A check-keyed side effect added to the
 *      handler, or to the helper without this list, turns the row red; the fix
 *      is to put it in the helper and name it here.
 *  (2) THE CALL SITES, each searched inside the function it claims: the handler
 *      runs the helper with the slot's check unless the slot is a show-only
 *      entry (read before the match frees it); the show-only release runs it
 *      after freeing the slot; the drain's foreign branch runs it in its toast
 *      path, after the record and before the pop.
 *  (3) THE BEHAVIOUR (games/oot/soh/oot_check_receive_effects_test.cpp): the
 *      Silver Gauntlets chest's show-only release marks Nabooru captured with
 *      the story skipped, arms the Colossus transition with it shown, once, and
 *      no other check's release does either.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * like its neighbours; every symbol under test is C-linkage.
 */

#include "../test_runner.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string>

extern "C" int OoT_CheckReceiveEffects_RunHeadless(void);

namespace {

int sOcrFailures = 0;

#define OCR_SRC_CHECK(cond, ...)                                   \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (oot-check-receive-effects): ");   \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sOcrFailures++;                                        \
        }                                                          \
    } while (0)

// Every check the receive path tests, as a literal (1).
const char* const kCheckKeyed[] = {
    "RC_SPIRIT_TEMPLE_SILVER_GAUNTLETS_CHEST",
};

const char* const kHookHandlers = "games/oot/soh/Enhancements/randomizer/hook_handlers.cpp";

std::string OcrReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
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
#else
    (void)rel;
    return std::string();
#endif
}

/** The body of the function whose definition starts with @p signature (the first
 *  occurrence followed by '{', not a prototype), braces matched; "" if absent. */
std::string OcrFunctionBody(const std::string& src, const char* signature) {
    size_t at = 0;
    while ((at = src.find(signature, at)) != std::string::npos) {
        size_t p = at + strlen(signature);
        while (p < src.size() && (src[p] == ' ' || src[p] == '\n' || src[p] == '\t')) {
            p++;
        }
        if (p < src.size() && src[p] == '{') {
            int depth = 0;
            for (size_t q = p; q < src.size(); q++) {
                if (src[q] == '{') {
                    depth++;
                } else if (src[q] == '}') {
                    depth--;
                    if (depth == 0) {
                        return src.substr(p, q - p + 1);
                    }
                }
            }
            return std::string();
        }
        at = p;
    }
    return std::string();
}

/** Every needle in order inside @p hay (false for an empty @p hay). */
bool OcrInOrder(const std::string& hay, std::initializer_list<const char*> needles) {
    if (hay.empty()) {
        return false;
    }
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

/** Every `RC_*` name compared with `==` or `!=` in @p body, RC_UNKNOWN_CHECK
 *  (the free slot) aside. */
std::set<std::string> OcrComparedChecks(const std::string& body) {
    std::set<std::string> out;
    for (const char* op : { "==", "!=" }) {
        size_t at = 0;
        while ((at = body.find(op, at)) != std::string::npos) {
            size_t p = at + 2;
            while (p < body.size() && body[p] == ' ') {
                p++;
            }
            if (body.compare(p, 3, "RC_") == 0) {
                size_t e = p;
                while (e < body.size() && (std::isalnum((unsigned char)body[e]) || body[e] == '_')) {
                    e++;
                }
                const std::string name = body.substr(p, e - p);
                if (name != "RC_UNKNOWN_CHECK") {
                    out.insert(name);
                }
            }
            at += 2;
        }
    }
    return out;
}

size_t OcrCount(const std::string& hay, const char* needle) {
    size_t n = 0;
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + 1)) {
        n++;
    }
    return n;
}

void OcrCheckSource() {
    const std::string src = OcrReadSource(kHookHandlers);
    OCR_SRC_CHECK(!src.empty(), "the source file %s could not be read (RSBS_SOURCE_DIR)\n", kHookHandlers);
    if (src.empty()) {
        return;
    }
    const std::string handler =
        OcrFunctionBody(src, "void RandomizerOnItemReceiveHandler(GetItemEntry receivedItemEntry)");
    const std::string helper = OcrFunctionBody(src, "static void RandomizerOnCheckReceived(RandomizerCheck rc)");
    const std::string release = OcrFunctionBody(src, "void Randomizer_ReleaseQueuedShowOnly(uint16_t rc)");
    const std::string drain = OcrFunctionBody(src, "void RandomizerOnPlayerUpdateForRCQueueHandler()");
    OCR_SRC_CHECK(!handler.empty() && !helper.empty() && !release.empty() && !drain.empty(),
                  "a function under test is missing from %s (handler %d, helper %d, release %d, drain %d)\n",
                  kHookHandlers, (int)!handler.empty(), (int)!helper.empty(), (int)!release.empty(),
                  (int)!drain.empty());

    // (1) The literal.
    std::set<std::string> literal;
    for (const char* rc : kCheckKeyed) {
        literal.insert(rc);
    }
    const std::set<std::string> inHelper = OcrComparedChecks(helper);
    const std::set<std::string> inHandler = OcrComparedChecks(handler);
    printf("[TEST] (1) RandomizerOnCheckReceived tests %zu check(s), the literal names %zu; the receive handler "
           "tests %zu check(s) and has %zu `GetRandomizerCheck() ==`\n",
           inHelper.size(), literal.size(), inHandler.size(), OcrCount(handler, "GetRandomizerCheck() =="));
    for (const std::string& rc : inHelper) {
        OCR_SRC_CHECK(literal.count(rc) == 1,
                      "(1) RandomizerOnCheckReceived tests %s, which kCheckKeyed does not name: add it to the "
                      "literal\n",
                      rc.c_str());
    }
    for (const std::string& rc : literal) {
        OCR_SRC_CHECK(inHelper.count(rc) == 1,
                      "(1) kCheckKeyed names %s, which RandomizerOnCheckReceived no longer tests\n", rc.c_str());
    }
    for (const std::string& rc : inHandler) {
        OCR_SRC_CHECK(false,
                      "(1) RandomizerOnItemReceiveHandler tests %s itself: a check-keyed side effect there never "
                      "runs for a check hosting a Majora's Mask item; move it into RandomizerOnCheckReceived\n",
                      rc.c_str());
    }
    OCR_SRC_CHECK(OcrCount(handler, "GetRandomizerCheck() ==") == 0 &&
                      OcrCount(handler, "GetRandomizerCheck() !=") == 0,
                  "(1) RandomizerOnItemReceiveHandler compares the slot's check itself: move that side effect into "
                  "RandomizerOnCheckReceived\n");

    // (2) The call sites.
    OCR_SRC_CHECK(OcrInOrder(handler, { "const bool isShowOnlySlot = OoT_Rando_Foreign_IsShowOnlyGetItem(",
                                        "randomizerQueuedCheck = RC_UNKNOWN_CHECK;", "if (!isShowOnlySlot) {",
                                        "RandomizerOnCheckReceived(loc->GetRandomizerCheck());" }),
                  "(2) the receive handler must read the show-only slot before the match frees it, and run the "
                  "helper with the slot's check only for a slot that is not show-only\n");
    OCR_SRC_CHECK(OcrInOrder(release, { "if (randomizerQueuedCheck == (RandomizerCheck)rc) {",
                                        "randomizerQueuedCheck = RC_UNKNOWN_CHECK;",
                                        "RandomizerOnCheckReceived((RandomizerCheck)rc);" }) &&
                      OcrCount(release, "RandomizerOnCheckReceived(") == 1,
                  "(2) the show-only release must run the helper once, inside its held-slot branch, after freeing "
                  "the slot\n");
    OCR_SRC_CHECK(
        OcrInOrder(drain, { "OoT_Rando_Foreign_RecordPickup((uint16_t)rc) != 0",
                            "RandomizerSkipForeignGetItemAnimation(rc) || !RandomizerQueueForeignShowOnly(rc)",
                            "OoT_Rando_Foreign_EmitPickupToast(", "RandomizerOnCheckReceived(rc);",
                            "randomizerQueuedChecks.pop();" }) &&
            OcrCount(drain, "RandomizerOnCheckReceived(") == 1,
        "(2) the drain's foreign branch must run the helper once, in its toast path, after the record and "
        "before the pop\n");
    printf("[TEST] (2) the handler, the show-only release and the drain's toast path run the helper where they "
           "claim\n");
}

} // namespace

TestResult OoTCheckReceiveEffects_Run(void) {
    printf("[TEST] oot-check-receive-effects: what OoT does because a check gave its item runs when that check hosts "
           "a Majora's Mask item too (#1602)\n");
    sOcrFailures = 0;
    OcrCheckSource();
    const int behaviour = OoT_CheckReceiveEffects_RunHeadless();
    if (behaviour != 0) {
        printf("[TEST] FAIL (oot-check-receive-effects): %d behaviour failure(s)\n", behaviour);
        sOcrFailures += behaviour;
    }
    printf("[TEST] oot-check-receive-effects: %s\n", sOcrFailures == 0 ? "PASS" : "FAIL");
    return sOcrFailures == 0 ? TEST_PASS : TEST_FAIL;
}
