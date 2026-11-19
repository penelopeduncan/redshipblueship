/**
 * @file oot_custom_message_text_test.cpp
 * #1187 locks: the message-text halves of SoH 983d04d36 that PR #1181 left out.
 *
 * ROW CustomMessageReplace (redship tier, no bring-up: CustomMessage is plain strings).
 *   THE DEFECT. CustomMessage::Replace searched again from position 0 after each
 *   replacement, so a replacement that contains the search string matched itself
 *   forever and the call never returned. AutoFormat(ItemID) is one such caller in
 *   production: it replaces WAIT_FOR_INPUT with WAIT_FOR_INPUT + the item icon, so
 *   any item text long enough to need a second page hung the game.
 *   R1. "banana", "a" -> "aa" reads "baanaanaa" in every language (the string overload).
 *   R2. the same through the CustomMessage overload.
 *   R3. a two-page message through AutoFormat(ITEM_DUNGEON_MAP) returns, and both
 *       pages open with the map icon.
 *   Each call runs on a worker thread under a watchdog: the defect never returns,
 *   so a call still running after the deadline is reported and the process exits
 *   with a failure code at once (the runaway thread would otherwise keep growing
 *   its string until the process ends).
 *   #1746 (SoH c9491183f, c9414b4d4), on the same plain strings:
 *   K1. Capitalize capitalises the first letter of every language (it looped over
 *       copies and changed nothing, so the altar text kept "the Lost Woods").
 *   K2. InsertNames capitalises only the names its capital flags ask for and leaves
 *       the flags as given (it tested capital[a] with an assignment).
 *   K3. GetAllMessages(MF_CLEAN) returns cleaned text (it looped over copies).
 *
 * ROW OoTItemText (redship tier; bring-up: the ROM-free item and hint tables, no
 * Rando::Context left behind). #1746 (SoH 86dea7d1b, 52a08daf0, 451820cac):
 *   T0. Text::IsMissing knows TODO_TRANSLATE's spelling.
 *   T1. anti-vacuity: 300+ named rows, 40+ missing German/French name slots,
 *       150+ French articles.
 *   T2. every named row reads a real name in English, German and French, and that
 *       name maps back to the row through itemNameToEnum (RG_SHADOW_SILVER_BLADES
 *       in German reads its English name).
 *   T3. "TranslateThis" is no item's key in itemNameToEnum.
 *   T4. FillMissingTranslations copies English into empty and placeholder slots.
 *   A1. no row has a German article in its French slot or a French one in its
 *       German slot.
 *   C1. every colour is empty or a %-code; the Water Temple Boss Key's is "%b".
 *   H1. every area and trial name, as the spoiler writes it, reads back through a
 *       fresh translation map (GetAllMessages formats now; its callers ask for
 *       MF_CLEAN).
 *
 * ROW MapCompassText (rando tier: it generates for the context BuildMapMessage reads).
 *   M1. a dungeon map's get-item text names the map ("Great Deku Tree Map"), never
 *       the "[[name]]" placeholder (BuildMapMessage never replaced it).
 *   M2. it is auto-formatted for the map: it opens with the map's item icon and
 *       ends with the message end code (BuildMapMessage never called AutoFormat).
 *   C1. every compass row of the item table opens TEXT_ITEM_COMPASS and every map
 *       row TEXT_ITEM_DUNGEON_MAP (the Great Deku Tree Compass opened the map's).
 *
 * RED: without the Replace fix R1-R3 time out; without the BuildMapMessage half M1
 * and M2 fail; without the item_list.cpp half C1 fails on RG_DEKU_TREE_COMPASS.
 * Without the #1746 fixes: K1-K3 fail on the by-value loops and the assignment;
 * T2 and T3 fail without the getter fallback or the getter-built keys; A1 fails on
 * any swapped article; C1 on the Water Temple Boss Key's "b"; H1 when a caller of
 * GetAllMessages takes its MF_FORMATTED default.
 */

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"

extern "C" {
#include <variables.h>
extern PlayState* OoT_gPlayState;
}

// 3drando/menu.cpp: the headless generation bridge every rando row uses.
extern "C" int Rando_HeadlessSeedTest(const char* seedStr);
// Messages/ItemMessages.cpp: dungeon map `rg`'s get-item text, raw English.
extern "C" int OoT_Rando_MapMessageText(uint16_t rg, char* out, size_t cap);
// Messages/ItemMessages.cpp: TEXT_RANDOMIZER_CUSTOM_ITEM OnOpenText handler.
void BuildItemMessage(u16* textId, bool* loadFromMessageTable);
// ComboLogicEngineOoT.cpp: the item table and itemNameToEnum, leaving no Rando::Context behind.
extern "C" int OoT_ComboLogic_TestEnsureItemTableTransient(void);
// HintAreaOoT.cpp: the hint text table without a ROM or a Rando::Context.
extern "C" int OoT_HintArea_TestInitStaticData(void);

namespace {

constexpr auto kCmtDeadline = std::chrono::seconds(10);

/**
 * Runs `work` on a worker thread and returns what it produced. When it has not
 * returned by the deadline the defect is the infinite loop: report it and exit the
 * process with a failure code, since the thread cannot be stopped.
 */
std::string CmtRunGuarded(const char* leg, std::function<std::string()> work) {
    struct State {
        std::mutex mutex;
        std::condition_variable done;
        bool finished = false;
        std::string result;
    };
    auto state = std::make_shared<State>();
    std::thread worker([state, work]() {
        std::string result = work();
        std::lock_guard<std::mutex> lock(state->mutex);
        state->result = std::move(result);
        state->finished = true;
        state->done.notify_one();
    });
    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->done.wait_for(lock, kCmtDeadline, [&state]() { return state->finished; })) {
        printf("[TEST] FAIL %s: CustomMessage::Replace did not return within %lld s; a replacement that contains "
               "the search string is matched again forever (#1187)\n",
               leg, (long long)kCmtDeadline.count());
        fflush(stdout);
        fflush(stderr);
        std::_Exit(1);
    }
    lock.unlock();
    worker.join();
    return state->result;
}

/** The three languages' raw text, joined with '|'. */
std::string CmtAllRaw(const CustomMessage& msg) {
    return msg.GetEnglish(MF_RAW) + "|" + msg.GetGerman(MF_RAW) + "|" + msg.GetFrench(MF_RAW);
}

/** Printable form of a raw message: control bytes as \xNN. */
std::string CmtPrintable(const std::string& raw) {
    std::string out;
    for (const unsigned char c : raw) {
        if (c < 0x20 || c >= 0x7F) {
            char hex[8];
            snprintf(hex, sizeof(hex), "\\x%02X", (unsigned)c);
            out += hex;
        } else {
            out += (char)c;
        }
    }
    return out;
}

size_t CmtCount(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    for (size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + needle.size())) {
        count++;
    }
    return count;
}

} // namespace

extern "C" int OoT_CustomMessageReplaceTest_Run(void) {
    printf("[TEST] custom-message-replace: CustomMessage::Replace returns when the replacement contains the search "
           "string (#1187)\n");
    int failures = 0;

    // R1. The string overload.
    const std::string r1 = CmtRunGuarded("R1", []() {
        CustomMessage msg("banana", "banana", "banana");
        msg.Replace("a", "aa");
        return CmtAllRaw(msg);
    });
    if (r1 != "baanaanaa|baanaanaa|baanaanaa") {
        printf("[TEST] FAIL R1: \"banana\" with \"a\" -> \"aa\" reads \"%s\", not \"baanaanaa\" in each language\n",
               r1.c_str());
        failures++;
    }

    // R2. The CustomMessage overload.
    const std::string r2 = CmtRunGuarded("R2", []() {
        CustomMessage msg("banana", "banana", "banana");
        msg.Replace("a", CustomMessage("aa", "aa", "aa"));
        return CmtAllRaw(msg);
    });
    if (r2 != "baanaanaa|baanaanaa|baanaanaa") {
        printf("[TEST] FAIL R2: the CustomMessage overload reads \"%s\", not \"baanaanaa\" in each language\n",
               r2.c_str());
        failures++;
    }

    // R3. AutoFormat(ItemID), the production caller: two pages, each opening with the icon.
    const std::string icon = CustomMessage::ITEM_OBTAINED(ITEM_DUNGEON_MAP);
    const std::string r3 = CmtRunGuarded("R3", []() {
        CustomMessage msg("You found the map!^It shows the whole dungeon.", TEXTBOX_TYPE_BLUE);
        msg.AutoFormat(ITEM_DUNGEON_MAP);
        return msg.GetEnglish(MF_RAW);
    });
    const std::string page = CustomMessage::WAIT_FOR_INPUT();
    if (r3.rfind(icon, 0) != 0 || CmtCount(r3, page + icon) != 1 || CmtCount(r3, icon) != 2) {
        printf("[TEST] FAIL R3: a two-page AutoFormat(ITEM_DUNGEON_MAP) reads \"%s\"; each page must open with the "
               "map icon exactly once\n",
               CmtPrintable(r3).c_str());
        failures++;
    }

    // R4. BuildItemMessage for RG_CRAWL auto-formats its custom message in all three languages (ends with MESSAGE_END
    // 0x02 and expands color/newline codes instead of leaving raw %r/%w/& without a MESSAGE_END terminator).
    {
        auto play = std::make_unique<PlayState>();
        auto player = std::make_unique<Player>();
        play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player->actor;
        player->getItemEntry.getItemId = RG_CRAWL;

        PlayState* prevPlay = OoT_gPlayState;
        const u8 prevLang = gSaveContext.language;
        OoT_gPlayState = play.get();

        const u8 langs[] = { LANGUAGE_ENG, LANGUAGE_GER, LANGUAGE_FRA };
        using namespace std::literals::string_literals;
        const std::string end = CustomMessage::MESSAGE_END();
        const std::string red = CustomMessage::COLOR(QM_RED);
        const std::string white = CustomMessage::COLOR(QM_WHITE);
        for (const u8 lang : langs) {
            gSaveContext.language = lang;
            u16 textId = TEXT_RANDOMIZER_CUSTOM_ITEM;
            bool loadFromMessageTable = true;
            BuildItemMessage(&textId, &loadFromMessageTable);
            const std::string raw(play->msgCtx.font.msgBuf, static_cast<size_t>(play->msgCtx.msgLength));
            if (loadFromMessageTable || raw.size() < end.size() ||
                raw.compare(raw.size() - end.size(), end.size(), end) != 0 || raw.find('%') != std::string::npos ||
                raw.find('&') != std::string::npos || raw.find(red) == std::string::npos ||
                raw.find(white) == std::string::npos) {
                printf("[TEST] FAIL R4 (lang=%u): RG_CRAWL BuildItemMessage produced \"%s\" (missing AutoFormat / "
                       "MESSAGE_END terminator)\n",
                       (unsigned)lang, CmtPrintable(raw).c_str());
                failures++;
            }
        }

        gSaveContext.language = prevLang;
        OoT_gPlayState = prevPlay;
    }

    // K1. Capitalize changes the strings themselves, in every language (#1746, SoH c9491183f).
    {
        CustomMessage msg("the Lost Woods", "in den Verlorenen Wäldern", "les Bois Perdus");
        msg.Capitalize();
        const std::string k1 = CmtAllRaw(msg);
        if (k1 != "The Lost Woods|In den Verlorenen Wäldern|Les Bois Perdus") {
            printf("[TEST] FAIL K1: Capitalize left \"%s\"; it must capitalise the first letter of each language "
                   "(#1746)\n",
                   k1.c_str());
            failures++;
        }
    }

    // K2. InsertNames capitalises only the names its flags ask for, and keeps the flags (#1746, SoH c9414b4d4).
    {
        CustomMessage msg("[[1]] / [[2]]", "[[1]] / [[2]]", "[[1]] / [[2]]", std::vector<std::string>{},
                          std::vector<bool>{ false, true });
        msg.InsertNames({ CustomMessage("the market", "der markt", "le marché"),
                          CustomMessage("the graveyard", "der friedhof", "le cimetière") });
        const std::string k2 = CmtAllRaw(msg);
        const std::vector<bool>& flags = msg.GetCapital();
        if (k2 != "the market / The graveyard|der markt / Der friedhof|le marché / Le cimetière" || flags.size() != 2 ||
            flags[0] || !flags[1]) {
            printf("[TEST] FAIL K2: InsertNames with flags {false, true} reads \"%s\" (flags %zu); only the second "
                   "name may be capitalised and the flags must stay as given (#1746)\n",
                   k2.c_str(), flags.size());
            failures++;
        }
    }

    // K3. GetAllMessages applies its format to what it returns (#1746, SoH c9491183f).
    {
        const CustomMessage msg("#Kokiri# Forest^", "#Kokiri#-Wald^", "#Forêt# Kokiri^");
        const std::vector<std::string> all = msg.GetAllMessages(MF_CLEAN);
        if (all.size() < 3 || all[LANGUAGE_ENG] != "Kokiri Forest " || all[LANGUAGE_GER] != "Kokiri-Wald " ||
            all[LANGUAGE_FRA] != "Forêt Kokiri ") {
            printf("[TEST] FAIL K3: GetAllMessages(MF_CLEAN) returned \"%s\"; it must clean each language's text "
                   "(#1746)\n",
                   all.empty() ? "" : all[LANGUAGE_ENG].c_str());
            failures++;
        }
    }

    if (failures != 0) {
        printf("[TEST] FAIL: custom-message-replace: %d assertion(s) failed\n", failures);
        return 1;
    }
    printf("[TEST] PASS: Replace returns and replaces every occurrence once, in both overloads and through "
           "AutoFormat(ItemID); Capitalize, InsertNames' flags and GetAllMessages' format take effect\n");
    return 0;
}

extern "C" int OoT_ItemTextTest_Run(void) {
    printf("[TEST] oot-item-text: missing translations fall back to English and round-trip through the name table; "
           "articles sit in their language's slot; colour codes are well-formed; area and trial names read back "
           "(#1746)\n");
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0 || OoT_HintArea_TestInitStaticData() != 0) {
        printf("[TEST] FAIL: OoT's item or hint table could not be brought up without a ROM\n");
        return 1;
    }
    int failures = 0;

    // T0. Text's own spelling of the placeholder matches TODO_TRANSLATE (text.hpp cannot include z64.h).
    if (!Text::IsMissing(TODO_TRANSLATE) || !Text::IsMissing("") || Text::IsMissing("Bomb")) {
        printf("[TEST] FAIL T0: Text::IsMissing does not recognise TODO_TRANSLATE (\"%s\") and \"\" only\n",
               TODO_TRANSLATE);
        failures++;
    }

    // T1-T3 over every named row of the item table; A1 and C1 over the same rows.
    static const char* const kOitGermanArticles[] = { "der", "die",  "das",   "den",   "dem",
                                                      "ein", "eine", "einen", "einem", "einer" };
    static const char* const kOitFrenchArticles[] = { "le", "la", "les", "l'", "un", "une", "du" };
    const auto oitTrimmedIn = [](const std::string& raw, const char* const* set, size_t count) {
        const size_t first = raw.find_first_not_of(' ');
        const std::string word =
            first == std::string::npos ? "" : raw.substr(first, raw.find_last_not_of(' ') - first + 1);
        for (size_t i = 0; i < count; i++) {
            if (word == set[i]) {
                return true;
            }
        }
        return false;
    };
    const uint8_t langs[] = { LANGUAGE_ENG, LANGUAGE_GER, LANGUAGE_FRA };
    int named = 0;
    int missingSlots = 0;
    int frenchArticles = 0;
    int t2Failures = 0;
    for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
        const Rando::Item& item = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rg));
        const Text& name = item.GetName();
        if (name.english.empty()) {
            continue;
        }
        named++;
        missingSlots += (Text::IsMissing(name.german) ? 1 : 0) + (Text::IsMissing(name.french) ? 1 : 0);
        for (const uint8_t lang : langs) {
            const std::string& shown = name.GetForLanguage(lang);
            const auto found = Rando::StaticData::itemNameToEnum.find(shown);
            if (shown.empty() || shown == TODO_TRANSLATE || found == Rando::StaticData::itemNameToEnum.end() ||
                found->second != static_cast<RandomizerGet>(rg)) {
                if (t2Failures++ < 5) {
                    printf("[TEST] FAIL T2: \"%s\" (%d) reads \"%s\" in language %u, which does not map back to it\n",
                           name.english.c_str(), rg, shown.c_str(), (unsigned)lang);
                }
            }
        }
        // A1. The raw French slot holds no German article and the raw German slot no French one.
        const Text& article = item.GetArticle();
        if (!article.french.empty() && article.french != TODO_TRANSLATE) {
            frenchArticles++;
        }
        if (oitTrimmedIn(article.french, kOitGermanArticles, std::size(kOitGermanArticles)) ||
            oitTrimmedIn(article.german, kOitFrenchArticles, std::size(kOitFrenchArticles))) {
            printf("[TEST] FAIL A1: \"%s\" (%d) has the article {\"%s\", \"%s\", \"%s\"}: French and German are "
                   "swapped\n",
                   name.english.c_str(), rg, article.english.c_str(), article.french.c_str(), article.german.c_str());
            failures++;
        }
        // C1. A colour is empty or a two-character %-code.
        const std::string& color = item.GetColor();
        if (!color.empty() && (color.size() != 2 || color[0] != '%')) {
            printf("[TEST] FAIL C1: \"%s\" (%d) has the colour code \"%s\"\n", name.english.c_str(), rg, color.c_str());
            failures++;
        }
    }
    failures += t2Failures;
    printf("[TEST] oot-item-text: %d named row(s), %d missing German/French name slot(s), %d French article(s)\n",
           named, missingSlots, frenchArticles);
    // T1. Anti-vacuity: the table is there, and missing translations remain to fall back from.
    if (named < 300 || missingSlots < 40 || frenchArticles < 150) {
        printf("[TEST] FAIL T1: too few rows to prove anything (need 300 named, 40 missing slots, 150 French "
               "articles)\n");
        failures++;
    }
    // T2, the case the issue names.
    const std::string blades = Rando::StaticData::RetrieveItem(RG_SHADOW_SILVER_BLADES).GetName().GetGerman();
    const auto bladesFound = Rando::StaticData::itemNameToEnum.find(blades);
    if (blades != "Shadow Silver: Blades" || bladesFound == Rando::StaticData::itemNameToEnum.end() ||
        bladesFound->second != RG_SHADOW_SILVER_BLADES) {
        printf("[TEST] FAIL T2: RG_SHADOW_SILVER_BLADES reads \"%s\" in German; it must fall back to its English "
               "name and map back to itself\n",
               blades.c_str());
        failures++;
    }
    // T3. The placeholder is not a key for any item (RG_NONE: an earlier spoiler parse may have asked for it).
    const auto placeholder = Rando::StaticData::itemNameToEnum.find(TODO_TRANSLATE);
    if (placeholder != Rando::StaticData::itemNameToEnum.end() && placeholder->second != RG_NONE) {
        printf("[TEST] FAIL T3: \"%s\" is a key of itemNameToEnum for item %d\n", TODO_TRANSLATE,
               (int)placeholder->second);
        failures++;
    }
    // T4. FillMissingTranslations copies English into empty and placeholder slots.
    {
        Text text{ "Bomb", TODO_TRANSLATE, "" };
        text.spanish = TODO_TRANSLATE;
        text.FillMissingTranslations();
        if (text.french != "Bomb" || text.german != "Bomb" || text.spanish != "Bomb") {
            printf("[TEST] FAIL T4: FillMissingTranslations left {\"%s\", \"%s\", \"%s\"}, not \"Bomb\" in each\n",
                   text.french.c_str(), text.german.c_str(), text.spanish.c_str());
            failures++;
        }
    }
    // C1, the row the fix names.
    if (Rando::StaticData::RetrieveItem(RG_WATER_TEMPLE_BOSS_KEY).GetColor() != "%b") {
        printf("[TEST] FAIL C1: the Water Temple Boss Key's colour is \"%s\", not \"%%b\"\n",
               Rando::StaticData::RetrieveItem(RG_WATER_TEMPLE_BOSS_KEY).GetColor().c_str());
        failures++;
    }

    // H1. Every area and trial name, as the spoiler writes it (clear text, MF_CLEAN), reads back through a fresh
    // translation map to an entry with that very name (#1746: GetAllMessages now formats, so its callers ask for
    // MF_CLEAN).
    int names = 0;
    const struct {
        const char* label;
        const std::unordered_map<uint32_t, RandomizerHintTextKey>* table;
    } kOitTables[] = { { "area", &Rando::StaticData::areaNames }, { "trial", &Rando::StaticData::trialData } };
    for (const auto& entry : kOitTables) {
        const std::unordered_map<std::string, uint32_t> map = Rando::StaticData::PopulateTranslationMap(*entry.table);
        for (const auto& [key, rht] : *entry.table) {
            const CustomMessage clear = Rando::StaticData::hintTextTable[rht].GetClear();
            for (const uint8_t lang : langs) {
                const std::string written = clear.GetForLanguage(lang, MF_CLEAN);
                names++;
                const auto found = map.find(written);
                const bool same =
                    found != map.end() &&
                    Rando::StaticData::hintTextTable[entry.table->at(found->second)].GetClear().GetForLanguage(
                        lang, MF_CLEAN) == written;
                if (!same) {
                    printf("[TEST] FAIL H1: the %s name \"%s\" (key %u, language %u) does not read back\n", entry.label,
                           written.c_str(), (unsigned)key, (unsigned)lang);
                    failures++;
                }
            }
        }
    }
    if (names < 90) {
        printf("[TEST] FAIL H1: only %d area and trial names checked\n", names);
        failures++;
    }

    if (failures != 0) {
        printf("[TEST] FAIL: oot-item-text: %d assertion(s) failed\n", failures);
        return 1;
    }
    printf("[TEST] PASS: missing translations fall back to English and round-trip; articles and colours are "
           "well-formed; area and trial names read back\n");
    return 0;
}

extern "C" int RandoTest_MapCompassText(void) {
    printf("[TEST] map-compass-text: a dungeon map's text names the map and is formatted; every compass opens the "
           "compass text (#1187)\n");
    if (Rando_HeadlessSeedTest("RSBSMAPTEXT1") != 0) {
        printf("[TEST] FAIL: headless generation failed, so there is no context to read the map text from\n");
        return 1;
    }
    int failures = 0;

    // M1 / M2: the Great Deku Tree Map's get-item text.
    // The raw text holds zero bytes (the default colour is "\x05\x00"), so it is read
    // by length, not as a C string.
    char text[2048] = {};
    const int length = OoT_Rando_MapMessageText(static_cast<uint16_t>(RG_DEKU_TREE_MAP), text, sizeof(text));
    if (length < 0) {
        printf("[TEST] FAIL: the map text bridge refused RG_DEKU_TREE_MAP\n");
        return 1;
    }
    const std::string raw(text, static_cast<size_t>(length));
    // The layout may break the name across lines: compare with each line break as a space.
    std::string words = raw;
    std::replace(words.begin(), words.end(), CustomMessage::NEWLINE()[0], ' ');
    const std::string name = Rando::StaticData::RetrieveItem(RG_DEKU_TREE_MAP).GetName().GetEnglish();
    printf("[TEST] map-compass-text: the Great Deku Tree Map reads \"%s\"\n", CmtPrintable(raw).c_str());
    if (name.empty() || words.find(name) == std::string::npos || raw.find("[[name]]") != std::string::npos) {
        printf("[TEST] FAIL M1: the map's text does not name \"%s\" (or still carries [[name]])\n", name.c_str());
        failures++;
    }
    const std::string icon = CustomMessage::ITEM_OBTAINED(ITEM_DUNGEON_MAP);
    const std::string end = CustomMessage::MESSAGE_END();
    if (raw.rfind(icon, 0) != 0 || raw.size() < end.size() ||
        raw.compare(raw.size() - end.size(), end.size(), end) != 0) {
        printf("[TEST] FAIL M2: the map's text is not auto-formatted: it must open with the map icon and end with "
               "the message end code\n");
        failures++;
    }

    // C1: every map and compass row of the item table.
    int maps = 0;
    int compasses = 0;
    for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
        const Rando::Item& item = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rg));
        const ItemType type = item.GetItemType();
        if (type != ITEMTYPE_MAP && type != ITEMTYPE_COMPASS) {
            continue;
        }
        const uint16_t want = type == ITEMTYPE_MAP ? TEXT_ITEM_DUNGEON_MAP : TEXT_ITEM_COMPASS;
        const uint16_t got = item.GetGIEntry_Copy().textId;
        (type == ITEMTYPE_MAP ? maps : compasses)++;
        if (got != want) {
            printf("[TEST] FAIL C1: \"%s\" (%d) opens text 0x%04X; a %s opens 0x%04X\n",
                   item.GetName().GetEnglish().c_str(), rg, (unsigned)got, type == ITEMTYPE_MAP ? "map" : "compass",
                   (unsigned)want);
            failures++;
        }
    }
    printf("[TEST] map-compass-text: %d map row(s), %d compass row(s) checked\n", maps, compasses);
    if (maps == 0 || compasses == 0) {
        printf("[TEST] FAIL C1: the item table has no map or no compass row, so C1 proves nothing\n");
        failures++;
    }

    if (failures != 0) {
        printf("[TEST] FAIL: map-compass-text: %d assertion(s) failed\n", failures);
        return 1;
    }
    printf("[TEST] PASS: the map's text names the map and opens with its icon; every compass opens the compass "
           "text\n");
    return 0;
}
