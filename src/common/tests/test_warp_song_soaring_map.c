/**
 * @file test_warp_song_soaring_map.c
 * @brief Locks for the Termina owl map OoT's Song of Soaring opens in its pause
 *        (#1169 lane XW8; the decisions are src/common/warp_song_soaring_map.c,
 *        OoT's draw and pause states games/oot/soh/Enhancements/randomizer/
 *        SoaringMapOoT.c, the guarded kaleido arms K1-K4).
 *
 * Two rows, both ROM-free (redship tier), neither generating nor touching any
 * game function, CVar or OTRGlobals:
 *
 *   soaring-map-picker (CTest SoaringMapPicker) -- THE PICKER, THE ROUTE AND THE
 *   PAUSE PRECONDITION. The picker opens only on an active owl and on MM's
 *   first owl; for every one of the 1024 owl masks it visits exactly the active
 *   owls in MM's index order, both ways, wrapping, and never an inactive one;
 *   the stick repeats exactly as MM's (frames 0, 11, 14, 17, 20), the D-pad
 *   only under DPadOnPause; every SELECT and CONFIRM input has MM's outcome
 *   (plus the stated advanceOther divergence); the closing events restore the
 *   HUD; every hand-back is OCARINA_MODE_04; the route sends the song to the
 *   map, the fallback chooser or the refusal; the pause precondition ignores
 *   the ocarina's own cutscene flag and refuses every other fact.
 *
 *   soaring-map-tables (CTest SoaringMapTables) -- MM'S PAGE, FROM MM'S SOURCE.
 *   The owl quads, cursor positions, cloud quads and name indices equal the
 *   tables in games/mm's z_kaleido_scope_NES.c and z_kaleido_map.c; every
 *   asset literal equals MM's own dg* define for the texture MM draws there;
 *   every asset is curated mm->oot (assets/crossgame/manifest.txt), so an
 *   OoT-first session finds it before MM's archive is mounted; none is an OoT
 *   path (textures/...).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * after test_warp_song_soaring.c; every symbol under test is C-linkage. Helpers
 * carry the Xw8 prefix because every included test file shares one TU.
 */

#include "../test_runner.h"
#include "../warp_song_soaring.h"
#include "../warp_song_soaring_map.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int sXw8Failures;

#define XW8_CHECK(cond, ...)                          \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            sXw8Failures++;                           \
        }                                             \
    } while (0)

namespace {

ComboSoaringMapInput Xw8Input(void) {
    ComboSoaringMapInput in;
    memset(&in, 0, sizeof(in));
    in.boxOpen = true;
    return in;
}

ComboSoaringMapEvent Xw8Move(ComboSoaringMap* map, int dir) {
    ComboSoaringMapInput in = Xw8Input();
    in.dir = (int8_t)dir;
    return Combo_SoaringMap_Step(map, &in);
}

ComboSoaringPauseFacts Xw8OpenFacts(void) {
    ComboSoaringPauseFacts f;
    memset(&f, 0, sizeof(f));
    f.cutsceneIndex = 0;
    f.nextCutsceneIndex = 0;
    f.shootingGalleryStatus = 1;
    f.magicStepCapacity = 2;
    f.magicFill = 3;
    f.magicState = 0;
    f.gameModeNormal = 0;
    f.gameMode = 0;
    return f;
}

std::string Xw8ReadSource(const char* rel) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#else
    (void)rel;
    return std::string();
#endif
}

std::string Xw8StripComments(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') {
                i++;
            }
            out.push_back('\n');
        } else if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            const size_t end = s.find("*/", i + 2);
            i = end == std::string::npos ? s.size() : end + 1;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

/** The comma-separated entries of the array whose declaration contains @p decl. */
std::vector<std::string> Xw8ArrayEntries(const std::string& src, const char* decl) {
    std::vector<std::string> out;
    const size_t at = src.find(decl);
    if (at == std::string::npos) {
        return out;
    }
    const size_t open = src.find('{', at);
    const size_t close = open == std::string::npos ? std::string::npos : src.find("};", open);
    if (close == std::string::npos) {
        return out;
    }
    const std::string body = Xw8StripComments(src.substr(open + 1, close - open - 1));
    std::string cur;
    for (char c : body) {
        if (c == ',') {
            out.push_back(cur);
            cur.clear();
        } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    std::vector<std::string> kept;
    for (const std::string& e : out) {
        if (!e.empty()) {
            kept.push_back(e);
        }
    }
    return kept;
}

/** MM's own path for asset @p name (its `#define dg<name> "..."`), "" if absent. */
std::string Xw8MMDefine(const std::string& headers, const std::string& name) {
    const std::string key = "#define dg" + name + " \"";
    const size_t at = headers.find(key);
    if (at == std::string::npos) {
        return std::string();
    }
    const size_t start = at + key.size();
    const size_t end = headers.find('"', start);
    return end == std::string::npos ? std::string() : headers.substr(start, end - start);
}

/** Whether @p path (without "__OTR__") is curated mm->oot, as a single path or
 *  under a directory entry. */
bool Xw8Curated(const std::string& manifest, const std::string& path) {
    size_t pos = 0;
    while (pos < manifest.size()) {
        size_t eol = manifest.find('\n', pos);
        if (eol == std::string::npos) {
            eol = manifest.size();
        }
        std::string line = manifest.substr(pos, eol - pos);
        pos = eol + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        static const char kPrefix[] = "mm->oot ";
        if (line.compare(0, sizeof(kPrefix) - 1, kPrefix) != 0) {
            continue;
        }
        const std::string entry = line.substr(sizeof(kPrefix) - 1);
        if (entry == path || (!entry.empty() && entry.back() == '/' && path.compare(0, entry.size(), entry) == 0)) {
            return true;
        }
    }
    return false;
}

const char kXw8Otr[] = "__OTR__";

} // namespace

// ============================================================================
// soaring-map-picker
// ============================================================================

TestResult Test_SoaringMapPicker(void) {
    printf("[TEST] soaring-map-picker: the owl map's picker offers exactly the active owls, moves as MM's, and every "
           "outcome, route and hand-back is MM's or OoTMM's (#1169 XW8)\n");
    sXw8Failures = 0;
    ComboSoaringMap map;

    // ---- (1) opening -------------------------------------------------------------------
    XW8_CHECK(!Combo_SoaringMap_Open(&map, 0) && map.phase == COMBO_SOARING_MAP_DONE && map.cursor < 0,
              "the map opened with no owl active");
    XW8_CHECK(!Combo_SoaringMap_Open(&map, 0x8000u), "the map opened on MM's hidden owl alone");
    for (int owl = 0; owl < RSBS_SOARING_OWL_COUNT; owl++) {
        XW8_CHECK(Combo_SoaringMap_Open(&map, (uint16_t)(1u << owl)) && map.cursor == owl &&
                      map.phase == COMBO_SOARING_MAP_SELECT && map.result == -1,
                  "one owl (%d) active: the map did not open on it (cursor %d)", owl, (int)map.cursor);
    }
    XW8_CHECK(Combo_SoaringMap_Open(&map, 0x83FFu) && map.mask == RSBS_SOARING_OWL_BITS &&
                  map.cursor == RSBS_SOARING_OWL_CLOCK_TOWN,
              "every owl and the hidden one: mask 0x%04X cursor %d (want the ten owls, Clock Town)", (unsigned)map.mask,
              (int)map.cursor);
    XW8_CHECK(Combo_SoaringMap_Open(&map, 0x0091u) && map.cursor == RSBS_SOARING_OWL_CLOCK_TOWN,
              "Clock Town active: the cursor did not start on it (MM func_800F4A10)");
    XW8_CHECK(Combo_SoaringMap_Open(&map, 0x0081u) && map.cursor == 0,
              "without Clock Town the cursor did not start on the lowest owl");

    // ---- (2) every mask: exactly the active owls, in order, both ways ----------------------
    {
        int bad = 0;
        for (uint32_t mask = 1; mask <= RSBS_SOARING_OWL_BITS && bad < 4; mask++) {
            for (int start = 0; start < RSBS_SOARING_OWL_COUNT; start++) {
                if (((mask >> start) & 1u) == 0) {
                    continue;
                }
                for (int dir = -1; dir <= 1; dir += 2) {
                    Combo_SoaringMap_Open(&map, (uint16_t)mask);
                    map.cursor = (int8_t)start;
                    int expect = start;
                    for (int step = 0; step < 20; step++) {
                        do {
                            expect = (expect + dir + RSBS_SOARING_OWL_COUNT) % RSBS_SOARING_OWL_COUNT;
                        } while (((mask >> expect) & 1u) == 0);
                        const ComboSoaringMapEvent ev = Xw8Move(&map, dir);
                        const bool single = (mask & (mask - 1u)) == 0;
                        if (map.cursor != expect || ((mask >> map.cursor) & 1u) == 0 ||
                            ev != (single ? COMBO_SOARING_MAP_EV_NONE : COMBO_SOARING_MAP_EV_MOVED)) {
                            if (bad++ < 4) {
                                printf("[TEST]   mask 0x%03X from %d dir %d step %d: cursor %d, want %d (event %d)\n",
                                       (unsigned)mask, start, dir, step, (int)map.cursor, expect, (int)ev);
                            }
                            break;
                        }
                    }
                }
            }
        }
        XW8_CHECK(bad == 0, "the cursor left the active owls or MM's index order (%d walks wrong)", bad);
    }
    XW8_CHECK(Combo_SoaringPrevOwl(0, 3) == -1 && Combo_SoaringPrevOwl(0x8000u, 3) == -1,
              "the previous owl of an empty mask was found");
    XW8_CHECK(Combo_SoaringPrevOwl(0x0081u, 0) == 7 && Combo_SoaringPrevOwl(0x0081u, 7) == 0,
              "left from Great Bay Coast did not wrap to the swamp");

    // ---- (3) MM's stick repeat, and the D-pad under DPadOnPause ------------------------------
    {
        Combo_SoaringMap_Open(&map, RSBS_SOARING_OWL_BITS);
        std::vector<int> fired;
        for (int frame = 0; frame <= 21; frame++) {
            if (Combo_SoaringMap_Stick(&map, 80, false, false, false) != 0) {
                fired.push_back(frame);
            }
        }
        const std::vector<int> want = { 0, 11, 14, 17, 20 };
        XW8_CHECK(fired == want, "a held stick registered on %zu frames, not 0, 11, 14, 17, 20 (MM's repeat)",
                  fired.size());
        XW8_CHECK(Combo_SoaringMap_Stick(&map, 0, false, false, false) == 0 &&
                      Combo_SoaringMap_Stick(&map, -80, false, false, false) == -1,
                  "a released then reversed stick did not register at once");
        XW8_CHECK(Combo_SoaringMap_Stick(&map, -80, false, false, false) == 0,
                  "the reversed stick repeated on its second frame");
        XW8_CHECK(Combo_SoaringMap_Stick(&map, 30, false, false, false) == 0 &&
                      Combo_SoaringMap_Stick(&map, -30, false, false, false) == 0,
                  "a stick at the threshold (|30|) moved the cursor (MM: past 30)");
        XW8_CHECK(Combo_SoaringMap_Stick(&map, 0, false, true, false) == 0 &&
                      Combo_SoaringMap_Stick(&map, 0, true, false, false) == 0,
                  "the D-pad moved the cursor with DPadOnPause off");
        XW8_CHECK(Combo_SoaringMap_Stick(&map, 0, false, true, true) == 1 &&
                      Combo_SoaringMap_Stick(&map, 0, true, false, true) == -1,
                  "the D-pad did not move the cursor with DPadOnPause on");
    }

    // ---- (4) every input's outcome ----------------------------------------------------------
    {
        ComboSoaringMapInput in = Xw8Input();
        Combo_SoaringMap_Open(&map, 0x0090u);
        in.b = true;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_CANCEL && map.result == -1 &&
                      map.phase == COMBO_SOARING_MAP_DONE,
                  "B on the map did not cancel with nothing chosen");
        Combo_SoaringMap_Open(&map, 0x0090u);
        in = Xw8Input();
        in.start = true;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_CANCEL && map.result == -1,
                  "START on the map did not cancel");
        Combo_SoaringMap_Open(&map, 0x0090u);
        in = Xw8Input();
        in.a = true;
        in.dir = 1;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_OPEN_CONFIRM &&
                      map.phase == COMBO_SOARING_MAP_CONFIRM && map.cursor == RSBS_SOARING_OWL_CLOCK_TOWN,
                  "A on the map did not open the prompt, or the A frame moved the cursor (MM: buttons first)");
        in = Xw8Input();
        in.dir = 1;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_NONE &&
                      map.cursor == RSBS_SOARING_OWL_CLOCK_TOWN && map.phase == COMBO_SOARING_MAP_CONFIRM,
                  "the stick moved the cursor under the prompt");
        in = Xw8Input();
        in.a = true;
        in.choice = 1;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_BACK &&
                      map.phase == COMBO_SOARING_MAP_SELECT,
                  "A on No did not go back to the map");
        XW8_CHECK(Xw8Move(&map, 1) == COMBO_SOARING_MAP_EV_MOVED && map.cursor == 7, "right from Clock Town did not "
                                                                                     "reach the swamp");
        static const char* const kBackNames[3] = { "B", "C-up (advanceOther)", "a vanished box" };
        for (int k = 0; k < 3; k++) {
            in = Xw8Input();
            in.a = true;
            Combo_SoaringMap_Step(&map, &in); // back into the prompt
            in = Xw8Input();
            if (k == 0) {
                in.b = true;
            } else if (k == 1) {
                in.advanceOther = true;
            } else {
                in.boxOpen = false;
            }
            XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_BACK &&
                          map.phase == COMBO_SOARING_MAP_SELECT && map.result == -1,
                      "%s under the prompt did not go back to the map", kBackNames[k]);
        }
        in = Xw8Input();
        in.a = true;
        Combo_SoaringMap_Step(&map, &in);
        in = Xw8Input();
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_NONE &&
                      map.phase == COMBO_SOARING_MAP_CONFIRM,
                  "an idle frame under an open prompt changed the phase");
        in.start = true;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_CANCEL && map.result == -1,
                  "START under the prompt did not cancel (MM NES.c CONFIRM START)");
        Combo_SoaringMap_Open(&map, 0x0090u);
        Xw8Move(&map, 1);
        in = Xw8Input();
        in.a = true;
        Combo_SoaringMap_Step(&map, &in);
        in.choice = 0;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_DECIDE && map.result == 7 &&
                      map.phase == COMBO_SOARING_MAP_DONE && Combo_SoaringOwlEntrance(map.result) == 0x84A0u,
                  "OK on the swamp did not decide the swamp owl (result %d)", (int)map.result);
        in = Xw8Input();
        in.b = true;
        in.a = true;
        in.dir = 1;
        XW8_CHECK(Combo_SoaringMap_Step(&map, &in) == COMBO_SOARING_MAP_EV_NONE && map.result == 7,
                  "a closed picker acted on input");
    }

    // ---- (5) the closes, the HUD and the hand-back ------------------------------------------
    for (int ev = COMBO_SOARING_MAP_EV_NONE; ev <= COMBO_SOARING_MAP_EV_DECIDE; ev++) {
        const bool closes = ev == COMBO_SOARING_MAP_EV_CANCEL || ev == COMBO_SOARING_MAP_EV_DECIDE;
        XW8_CHECK(Combo_SoaringMap_EventCloses((ComboSoaringMapEvent)ev) == closes, "event %d closes %d, want %d", ev,
                  Combo_SoaringMap_EventCloses((ComboSoaringMapEvent)ev) ? 1 : 0, closes ? 1 : 0);
        XW8_CHECK(Combo_SoaringMap_HudRestoreOnEvent((ComboSoaringMapEvent)ev) ==
                      (closes ? (uint16_t)RSBS_OOT_HUD_RESTORE_ALL : (uint16_t)0),
                  "event %d restores HUD 0x%X (every close must write 0x32, the prompt re-snapshots unk_13EE)", ev,
                  (unsigned)Combo_SoaringMap_HudRestoreOnEvent((ComboSoaringMapEvent)ev));
    }
    XW8_CHECK(Combo_SoaringMap_OcarinaModeOnHandBack() == RSBS_OOT_OCARINA_MODE_END &&
                  Combo_SoaringMap_OcarinaModeOnHandBack() == 4u,
              "the hand-back leaves ocarina mode %u, not 04 (02 would warp through sWarpSongEntrances)",
              (unsigned)Combo_SoaringMap_OcarinaModeOnHandBack());

    // ---- (6) the route --------------------------------------------------------------------------
    {
        static const ComboSoaringGate kGates[4] = { COMBO_SOARING_GATE_OFFER, COMBO_SOARING_GATE_RESTRICTED,
                                                    COMBO_SOARING_GATE_MM_UNPLAYABLE, COMBO_SOARING_GATE_NO_STATUES };
        for (int g = 0; g < 4; g++) {
            for (int assets = 0; assets <= 1; assets++) {
                for (int pause = 0; pause <= 1; pause++) {
                    const ComboSoaringRoute want = g != 0              ? COMBO_SOARING_ROUTE_REFUSE
                                                   : (assets && pause) ? COMBO_SOARING_ROUTE_MAP
                                                                       : COMBO_SOARING_ROUTE_CHOOSER;
                    XW8_CHECK(Combo_SoaringMap_Route(kGates[g], assets != 0, pause != 0) == want,
                              "gate %d, assets %d, pause %d routed %d, want %d", g, assets, pause,
                              (int)Combo_SoaringMap_Route(kGates[g], assets != 0, pause != 0), (int)want);
                }
            }
        }
    }

    // ---- (7) the pause precondition ---------------------------------------------------------------
    {
        ComboSoaringPauseFacts f = Xw8OpenFacts();
        XW8_CHECK(Combo_SoaringMap_PauseCanOpen(&f),
                  "the ocarina-out facts (no cutscene state, no player cutscene action) refused the pause");
        XW8_CHECK(!Combo_SoaringMap_PauseCanOpen(NULL), "no facts opened the pause");
        struct Xw8Flip {
            const char* what;
            int32_t ComboSoaringPauseFacts::*field;
            int32_t value;
        };
        static const Xw8Flip kFlips[] = {
            { "a pause already open", &ComboSoaringPauseFacts::pauseState, 1 },
            { "the debug editor", &ComboSoaringPauseFacts::debugState, 1 },
            { "a game over", &ComboSoaringPauseFacts::gameOverState, 1 },
            { "a transition trigger", &ComboSoaringPauseFacts::transitionTrigger, 1 },
            { "a transition mode", &ComboSoaringPauseFacts::transitionMode, 1 },
            { "a cutscene", &ComboSoaringPauseFacts::csState, 1 },
            { "a cutscene index", &ComboSoaringPauseFacts::cutsceneIndex, 0xFFF0 },
            { "a next cutscene index", &ComboSoaringPauseFacts::nextCutsceneIndex, 0xFFF1 },
            { "the shooting gallery", &ComboSoaringPauseFacts::shootingGalleryStatus, 2 },
            { "magic stepping its capacity", &ComboSoaringPauseFacts::magicState, 2 },
            { "magic filling", &ComboSoaringPauseFacts::magicState, 3 },
            { "a game mode other than normal", &ComboSoaringPauseFacts::gameMode, 1 },
            { "a player cutscene action", &ComboSoaringPauseFacts::playerCsAction, 5 },
            { "a dead Link", &ComboSoaringPauseFacts::playerDead, 1 },
        };
        for (const Xw8Flip& flip : kFlips) {
            f = Xw8OpenFacts();
            f.*(flip.field) = flip.value;
            XW8_CHECK(!Combo_SoaringMap_PauseCanOpen(&f), "%s did not refuse the pause", flip.what);
        }
    }

    // ---- (8) the pause states ------------------------------------------------------------------------
    {
        static const uint16_t kMapStates[7] = { RSBS_OOT_PAUSE_SOARING_0,      RSBS_OOT_PAUSE_SOARING_1,
                                                RSBS_OOT_PAUSE_SOARING_2,      RSBS_OOT_PAUSE_SOARING_3,
                                                RSBS_OOT_PAUSE_SOARING_SELECT, RSBS_OOT_PAUSE_SOARING_CONFIRM,
                                                RSBS_OOT_PAUSE_SOARING_6 };
        for (uint16_t s = 0; s <= RSBS_OOT_PAUSE_UNPAUSE_CLOSE; s++) {
            XW8_CHECK(!Combo_SoaringMap_IsMapState(s) && !Combo_SoaringMap_DrawsState(s) &&
                          !Combo_SoaringMap_OwnsUpdate(s, false),
                      "vanilla pause state 0x%X is taken by the map", (unsigned)s);
        }
        XW8_CHECK(Combo_SoaringMap_OwnsUpdate(RSBS_OOT_PAUSE_UNPAUSE_SETUP, true) &&
                      !Combo_SoaringMap_OwnsUpdate(RSBS_OOT_PAUSE_UNPAUSE_CLOSE, true),
                  "the map's close must own 0x12's one frame and leave 0x13 to vanilla");
        for (int i = 0; i < 7; i++) {
            XW8_CHECK(kMapStates[i] > RSBS_OOT_PAUSE_UNPAUSE_CLOSE && Combo_SoaringMap_IsMapState(kMapStates[i]) &&
                          Combo_SoaringMap_OwnsUpdate(kMapStates[i], false) &&
                          Combo_SoaringMap_DrawsState(kMapStates[i]) == (i >= 2),
                      "map state 0x%X is not the map's, or its draw gate is wrong", (unsigned)kMapStates[i]);
        }
    }

    printf("[TEST] soaring-map-picker: %s\n", sXw8Failures == 0 ? "PASS" : "FAIL");
    return sXw8Failures == 0 ? TEST_PASS : TEST_FAIL;
}

// ============================================================================
// soaring-map-tables
// ============================================================================

TestResult Test_SoaringMapTables(void) {
    printf("[TEST] soaring-map-tables: the owl map's positions, clouds, names and resource paths are MM's own, and "
           "every asset is curated for OoT (#1169 XW8)\n");
    sXw8Failures = 0;

    const std::string nes =
        Xw8ReadSource("games/mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c");
    const std::string mapc = Xw8ReadSource("games/mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_map.c");
    const std::string items = Xw8ReadSource("games/mm/include/z64item.h");
    const std::string headers =
        Xw8ReadSource("games/mm/assets/interface/icon_item_field_static/icon_item_field_static.h") +
        Xw8ReadSource("games/mm/assets/interface/icon_item_jpn_static/icon_item_jpn_static.h") +
        Xw8ReadSource("games/mm/assets/archives/map_name_static/map_name_static.h");
    const std::string manifest = Xw8ReadSource("assets/crossgame/manifest.txt");
    if (nes.empty() || mapc.empty() || items.empty() || headers.empty() || manifest.empty()) {
        XW8_CHECK(false, "MM's kaleido sources, its asset headers or the manifest are not readable under "
                         "RSBS_SOURCE_DIR");
        printf("[TEST] soaring-map-tables: FAIL\n");
        return TEST_FAIL;
    }

    // ---- (1) the quads: MM's world-map rows (clouds 0..14, owls 26..35) ---------------------
    const std::vector<std::string> qx = Xw8ArrayEntries(nes, "s16 sVtxPageMapWorldQuadsX[");
    const std::vector<std::string> qy = Xw8ArrayEntries(nes, "s16 sVtxPageMapWorldQuadsY[");
    const std::vector<std::string> qw = Xw8ArrayEntries(nes, "s16 gVtxPageMapWorldQuadsWidth[");
    const std::vector<std::string> qh = Xw8ArrayEntries(nes, "s16 gVtxPageMapWorldQuadsHeight[");
    XW8_CHECK(qx.size() == 36 && qy.size() == 36 && qw.size() == 36 && qh.size() == 36,
              "MM's world-map quad tables have %zu/%zu/%zu/%zu rows, not 36", qx.size(), qy.size(), qw.size(),
              qh.size());
    if (qx.size() == 36 && qy.size() == 36 && qw.size() == 36 && qh.size() == 36) {
        for (int n = 0; n < RSBS_SOARING_CLOUD_COUNT; n++) {
            ComboSoaringQuad q;
            XW8_CHECK(Combo_SoaringMap_CloudQuad(n, &q) && q.x == atoi(qx[n].c_str()) && q.y == atoi(qy[n].c_str()) &&
                          q.w == atoi(qw[n].c_str()) && q.h == atoi(qh[n].c_str()),
                      "cloud %d's quad is not MM's row %d", n, n);
        }
        for (int owl = 0; owl < RSBS_SOARING_OWL_COUNT; owl++) {
            ComboSoaringQuad q;
            const int row = 26 + owl;
            XW8_CHECK(Combo_SoaringMap_OwlQuad(owl, &q) && q.x == atoi(qx[row].c_str()) &&
                          q.y == atoi(qy[row].c_str()) && q.w == atoi(qw[row].c_str()) && q.h == atoi(qh[row].c_str()),
                      "owl %d's face quad is not MM's row %d", owl, row);
        }
    }
    {
        ComboSoaringQuad q;
        XW8_CHECK(!Combo_SoaringMap_OwlQuad(-1, &q) && !Combo_SoaringMap_OwlQuad(RSBS_SOARING_OWL_COUNT, &q) &&
                      !Combo_SoaringMap_CloudQuad(RSBS_SOARING_CLOUD_COUNT, &q) &&
                      Combo_SoaringMap_CloudTex(RSBS_SOARING_CLOUD_COUNT) == NULL &&
                      Combo_SoaringMap_OwlNameTex(RSBS_SOARING_OWL_COUNT) == NULL &&
                      Combo_SoaringMap_OwlNameIndex(-1) == -1,
                  "an out-of-range owl or cloud has data");
    }

    // ---- (2) the cursor positions ------------------------------------------------------------------
    const std::vector<std::string> cx = Xw8ArrayEntries(nes, "f32 sOwlWarpWorldMapCursorsX[");
    const std::vector<std::string> cy = Xw8ArrayEntries(nes, "f32 sOwlWarpWorldMapCursorsY[");
    XW8_CHECK(cx.size() == RSBS_SOARING_OWL_COUNT && cy.size() == RSBS_SOARING_OWL_COUNT,
              "MM's owl cursor tables have %zu/%zu rows", cx.size(), cy.size());
    if (cx.size() == RSBS_SOARING_OWL_COUNT && cy.size() == RSBS_SOARING_OWL_COUNT) {
        for (int owl = 0; owl < RSBS_SOARING_OWL_COUNT; owl++) {
            float x = 0.0f;
            float y = 0.0f;
            XW8_CHECK(Combo_SoaringMap_OwlCursor(owl, &x, &y) && std::fabs(x - strtof(cx[owl].c_str(), NULL)) < 0.01f &&
                          std::fabs(y - strtof(cy[owl].c_str(), NULL)) < 0.01f,
                      "owl %d's cursor (%.1f, %.1f) is not MM's (%s, %s)", owl, x, y, cx[owl].c_str(), cy[owl].c_str());
        }
    }

    // ---- (3) the names: sOwlWarpPauseItems through MM's item enum and its name list ---------------
    const std::vector<std::string> pauseItems = Xw8ArrayEntries(mapc, "u16 sOwlWarpPauseItems[");
    const std::vector<std::string> nameList = Xw8ArrayEntries(nes, "static const char* gMapNameStatics[");
    XW8_CHECK(pauseItems.size() == RSBS_SOARING_OWL_COUNT && nameList.size() == 16,
              "MM's sOwlWarpPauseItems has %zu rows and gMapNameStatics %zu", pauseItems.size(), nameList.size());
    if (pauseItems.size() == RSBS_SOARING_OWL_COUNT && nameList.size() == 16) {
        for (int owl = 0; owl < RSBS_SOARING_OWL_COUNT; owl++) {
            // "/* 0xAF */ ITEM_MAP_POINT_GREAT_BAY_COAST," gives the value.
            const std::string key = " */ " + pauseItems[owl] + ",";
            const size_t at = items.find(key);
            const size_t hex = at == std::string::npos ? std::string::npos : items.rfind("0x", at);
            const int value = hex == std::string::npos ? -1 : (int)strtol(items.c_str() + hex, NULL, 16);
            const int index = value - 0xA4; // ITEM_MAP_POINT_GREAT_BAY
            XW8_CHECK(index >= 0 && index < 16 && Combo_SoaringMap_OwlNameIndex(owl) == index,
                      "owl %d's name index is %d, MM's %s gives %d", owl, Combo_SoaringMap_OwlNameIndex(owl),
                      pauseItems[owl].c_str(), index);
            if (index >= 0 && index < 16) {
                // gMapPoint...Tex is defined as dgMapPoint...Tex.
                const std::string mmPath = Xw8MMDefine(headers, nameList[index].substr(1));
                const char* ours = Combo_SoaringMap_OwlNameTex(owl);
                XW8_CHECK(ours != NULL && !mmPath.empty() && mmPath == ours,
                          "owl %d's name texture is '%s', MM's %s is '%s'", owl, ours ? ours : "(null)",
                          nameList[index].c_str(), mmPath.c_str());
            }
        }
    }

    // ---- (4) the clouds, the image, the title and the face: MM's own defines -------------------
    const std::vector<std::string> clouds = Xw8ArrayEntries(mapc, "TexturePtr sCloudTextures[");
    XW8_CHECK(clouds.size() == RSBS_SOARING_CLOUD_COUNT, "MM's sCloudTextures has %zu rows", clouds.size());
    for (size_t n = 0; n < clouds.size() && n < (size_t)RSBS_SOARING_CLOUD_COUNT; n++) {
        const std::string mmPath = Xw8MMDefine(headers, clouds[n].substr(1));
        const char* ours = Combo_SoaringMap_CloudTex((int)n);
        XW8_CHECK(ours != NULL && !mmPath.empty() && mmPath == ours, "cloud %zu is '%s', MM's %s is '%s'", n,
                  ours ? ours : "(null)", clouds[n].c_str(), mmPath.c_str());
    }
    XW8_CHECK(Xw8MMDefine(headers, "WorldMapImageTex") == Combo_SoaringMap_WorldImageTex() &&
                  Xw8MMDefine(headers, "WorldMapImageTLUT") == Combo_SoaringMap_WorldImageTlut() &&
                  Xw8MMDefine(headers, "WorldMapOwlFaceTex") == Combo_SoaringMap_OwlFaceTex() &&
                  Xw8MMDefine(headers, "PauseMap10ENGTex") == Combo_SoaringMap_TitleTex(),
              "the world map image, its TLUT, the owl face or the map title is not MM's own path");

    // ---- (5) every asset: MM's, curated for OoT, never an OoT path ---------------------------
    {
        size_t count = 0;
        const char* const* paths = Combo_SoaringMap_AssetPaths(&count);
        XW8_CHECK(paths != NULL && count == 4u + RSBS_SOARING_CLOUD_COUNT + RSBS_SOARING_OWL_COUNT,
                  "the map lists %zu assets, not the image, TLUT, face, title, 15 clouds and 10 names", count);
        for (size_t i = 0; paths != NULL && i < count; i++) {
            const std::string p = paths[i] != NULL ? paths[i] : "";
            const bool otr = p.compare(0, sizeof(kXw8Otr) - 1, kXw8Otr) == 0;
            const std::string bare = otr ? p.substr(sizeof(kXw8Otr) - 1) : p;
            XW8_CHECK(otr && headers.find("\"" + p + "\"") != std::string::npos,
                      "asset '%s' is not one of MM's own defines", p.c_str());
            XW8_CHECK(bare.compare(0, 9, "textures/") != 0, "asset '%s' is an OoT path", p.c_str());
            XW8_CHECK(Xw8Curated(manifest, bare),
                      "asset '%s' is not curated mm->oot: an OoT-first session cannot draw it before MM's archive is "
                      "mounted",
                      bare.c_str());
        }
        // Every name and cloud the draw reaches is in the probed list.
        for (int owl = 0; owl < RSBS_SOARING_OWL_COUNT; owl++) {
            bool listed = false;
            for (size_t i = 0; i < count; i++) {
                listed = listed || paths[i] == Combo_SoaringMap_OwlNameTex(owl);
            }
            XW8_CHECK(listed, "owl %d's name texture is drawn but not probed", owl);
        }
        for (int n = 0; n < RSBS_SOARING_CLOUD_COUNT; n++) {
            bool listed = false;
            for (size_t i = 0; i < count; i++) {
                listed = listed || paths[i] == Combo_SoaringMap_CloudTex(n);
            }
            XW8_CHECK(listed, "cloud %d is drawn but not probed", n);
        }
    }

    printf("[TEST] soaring-map-tables: %s\n", sXw8Failures == 0 ? "PASS" : "FAIL");
    return sXw8Failures == 0 ? TEST_PASS : TEST_FAIL;
}
