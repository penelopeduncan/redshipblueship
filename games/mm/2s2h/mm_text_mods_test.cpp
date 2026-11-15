/**
 * @file mm_text_mods_test.cpp
 * @brief ROM-free lock for Majora's Mask text mods (2Ship #2000, #2013): a TextMM XML resource under
 *        override/<a message table's folder>/ replaces or adds entries in that table.
 *
 * CTest row MMTextMods in CMake/SingleExecutable.cmake, dispatch "mm-text-mods" in src/common/test_runner.cpp.
 *
 * WHAT IS UNDER TEST. MM_OTRMessage_Init builds MM's message table from text/message_data_static/... through
 * OTRMessage_LoadTable (z_message_OTR.cpp). Since 2Ship #2000 that function also loads every resource under
 * override/text/message_data_static/ and upserts its entries, applied lowest archive priority first so the
 * highest-priority mod wins, and the XML TextMM reader (version 1, registered in RegisterMMResourceFactories)
 * encodes the mod's markup into MM's message bytes. The row stages two loose folders as archives (the second mounted
 * last, so it has the higher priority), builds a table from its own path through MM_OTRMessage_LoadTableHeadless
 * (the same loader, with a path no real archive carries) and reads the bytes back.
 *
 * WHAT IT ASSERTS:
 *   1. The XML base table loads at all (the XML factory is registered) and its first entry's header and body are
 *      the encoding of its markup: box type, Centered, position, skip, icon, next message, both prices, a text run,
 *      a wrapped colour that closes back to the default, a newline, a button, a two-byte Delay argument and the
 *      End MM_Message_FindMessage's callers stop at.
 *   2. An entry two override files both carry takes the text of the one in the higher-priority archive, although
 *      that file's path sorts first.
 *   3. An entry only an override carries is added BEFORE the 0xFFFF terminator, which stays last.
 *   4. An override 0xFFFF entry, an over-long message, and a file that is another game's resource type (OoT's
 *      SOH::Text layout, root <Text>) change nothing.
 * RED without the XML registration: leg 1 (the base table does not load). RED without the priority sort: leg 2.
 * RED with the add appended after the terminator: leg 3.
 *
 * Restores the archive list it found and unloads every resource it loaded, on every exit.
 */

#include <cstdint>
#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_TextMods_RunHeadless(void) {
    printf("[TEST] mm-text-mods: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "test_scratch.h" // src/common: a scratch directory per process

extern "C" {
#include <message_data_static.h>
MessageTableEntry* MM_OTRMessage_LoadTableHeadless(const char* filePath);
int MM_RegisterResourceFactoriesHeadless(void);
}

// Last: its MESSAGE_* constants are what the row compares bytes against, and its short helper macros (HEX, STR)
// stay out of every header above.
#include "message_data_fmt_nes.h"

namespace {

constexpr const char* kTmBasePath = "rsbs-test/mm-text-mods/table/message_data_static";
constexpr const char* kTmOverrideDir = "override/rsbs-test/mm-text-mods/table";

bool TmWriteFile(const std::filesystem::path& path, const std::string& bytes) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    out << bytes;
    return out.good();
}

std::string TmBody(const MessageTableEntry& entry) {
    if (entry.segment == nullptr || entry.msgSize < 11) {
        return std::string();
    }
    return std::string(entry.segment + 11, entry.segment + entry.msgSize);
}

const MessageTableEntry* TmFind(const std::vector<MessageTableEntry>& entries, uint16_t id) {
    for (const auto& entry : entries) {
        if (entry.textId == id) {
            return &entry;
        }
    }
    return nullptr;
}

std::string TmHex(const std::string& bytes) {
    std::string out;
    char byte[4];
    for (unsigned char c : bytes) {
        snprintf(byte, sizeof(byte), "%02X ", c);
        out += byte;
    }
    return out;
}

} // namespace

extern "C" int MM_TextMods_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    auto rm = ctx ? ctx->GetResourceManager() : nullptr;
    auto am = rm ? rm->GetArchiveManager() : nullptr;
    if (am == nullptr) {
        printf("[mm-text-mods] FAIL: no ResourceManager/ArchiveManager after the bring-up\n");
        return 1;
    }
    if (MM_RegisterResourceFactoriesHeadless() != 0) {
        printf("[mm-text-mods] FAIL: MM's resource factories could not be registered\n");
        return 1;
    }

    const std::filesystem::path dir = rsbs::test::ScratchDir("mm-text-mods");
    const std::filesystem::path low = dir / "tm-low";
    const std::filesystem::path high = dir / "tm-high";
    const std::string overrideDir = kTmOverrideDir;
    const std::string overrideLongText(1300, 'a');

    const bool staged =
        TmWriteFile(low / kTmBasePath,
                    "<TextMM Version=\"1\">\n"
                    "  <TextEntry ID=\"0x0001\" Type=\"Sign\" Centered=\"true\" Position=\"Bottom\" "
                    "Skip=\"Unskippable\" Icon=\"0x10\" Next=\"0x0002\" FirstPrice=\"20\" SecondPrice=\"0x0005\">"
                    "Hi <Color Value=\"Red\">red</Color> x<NewLine/><Button Value=\"A\"/><Delay Value=\"0x0102\"/>"
                    "</TextEntry>\n"
                    "  <TextEntry ID=\"0x0002\">Two</TextEntry>\n"
                    "  <TextEntry ID=\"0xFFFF\"></TextEntry>\n"
                    "</TextMM>\n") &&
        TmWriteFile(low / overrideDir / "b_low", "<TextMM Version=\"1\">\n"
                                                 "  <TextEntry ID=\"0x0002\">Low</TextEntry>\n"
                                                 "  <TextEntry ID=\"0x0003\">Added</TextEntry>\n"
                                                 "  <TextEntry ID=\"0xFFFF\">X</TextEntry>\n"
                                                 "  <TextEntry ID=\"0x0005\">" +
                                                     overrideLongText +
                                                     "</TextEntry>\n"
                                                     "</TextMM>\n") &&
        TmWriteFile(low / overrideDir / "c_oot", "<Text Version=\"0\">\n"
                                                 "  <TextEntry ID=\"4\" TextboxType=\"0\" TextboxYPos=\"0\" "
                                                 "Message=\"OoT\"/>\n"
                                                 "</Text>\n") &&
        TmWriteFile(high / overrideDir / "a_high", "<TextMM Version=\"1\">\n"
                                                   "  <TextEntry ID=\"0x0002\">High</TextEntry>\n"
                                                   "</TextMM>\n");
    if (!staged) {
        printf("[mm-text-mods] FAIL: could not stage the folders under %s\n", dir.generic_string().c_str());
        return 1;
    }

    const std::vector<std::string> loaded = { kTmBasePath, overrideDir + "/a_high", overrideDir + "/b_low",
                                              overrideDir + "/c_oot" };
    auto snapshot = am->GetArchives();
    auto restore = [&]() {
        for (const auto& path : loaded) {
            rm->UnloadResource(path);
        }
        am->SetArchives(snapshot);
    };

    // The higher-priority archive is the one mounted last.
    if (am->AddArchive(low.generic_string()) == nullptr || am->AddArchive(high.generic_string()) == nullptr) {
        printf("[mm-text-mods] FAIL: the staged folders could not be mounted\n");
        restore();
        return 1;
    }

    int failures = 0;
    MessageTableEntry* table = MM_OTRMessage_LoadTableHeadless(kTmBasePath);
    std::vector<MessageTableEntry> entries;
    if (table == nullptr) {
        printf("[mm-text-mods] FAIL: leg 1: the XML base table %s did not load (is the TextMM XML factory, version "
               "1, registered in RegisterMMResourceFactories?)\n",
               kTmBasePath);
        restore();
        return 1;
    }
    for (size_t i = 0; i < 16; i++) {
        entries.push_back(table[i]);
        if (table[i].textId == 0xFFFF) {
            break;
        }
    }

    // 1. The base entry's header and body are its markup's encoding.
    const MessageTableEntry* first = TmFind(entries, 0x0001);
    const std::string expectedHeader = { 0x11, 0x31, 0x10, 0x00, 0x02, 0x00, 20, 0x00, 0x05, (char)0xFF, (char)0xFF };
    const std::string expectedBody = std::string("Hi ") + (char)MESSAGE_COLOR_RED + "red" +
                                     (char)MESSAGE_COLOR_DEFAULT + " x" + (char)MESSAGE_NEWLINE + (char)MESSAGE_BTN_A +
                                     (char)MESSAGE_DELAY + (char)0x01 + (char)0x02 + (char)MESSAGE_END;
    if (first == nullptr || first->segment == nullptr || first->msgSize < 11) {
        printf("[mm-text-mods] FAIL: leg 1: entry 0x0001 is missing from the table\n");
        failures++;
    } else {
        const std::string header(first->segment, first->segment + 11);
        if (header != expectedHeader || TmBody(*first) != expectedBody) {
            printf("[mm-text-mods] FAIL: leg 1: entry 0x0001 encodes as header %s body %s, expected header %s body "
                   "%s\n",
                   TmHex(header).c_str(), TmHex(TmBody(*first)).c_str(), TmHex(expectedHeader).c_str(),
                   TmHex(expectedBody).c_str());
            failures++;
        }
    }

    // 2. The higher-priority archive's override wins although its path sorts first.
    const MessageTableEntry* second = TmFind(entries, 0x0002);
    const std::string secondBody = second != nullptr ? TmBody(*second) : std::string("(missing)");
    if (secondBody != std::string("High") + (char)MESSAGE_END) {
        printf("[mm-text-mods] FAIL: leg 2: entry 0x0002 reads %s, expected the higher-priority archive's \"High\"\n",
               TmHex(secondBody).c_str());
        failures++;
    }

    // 3. An added entry sits before the terminator, which stays last.
    const MessageTableEntry* added = TmFind(entries, 0x0003);
    if (added == nullptr || TmBody(*added) != std::string("Added") + (char)MESSAGE_END) {
        printf("[mm-text-mods] FAIL: leg 3: entry 0x0003 is not in the table before 0xFFFF\n");
        failures++;
    }
    if (entries.empty() || entries.back().textId != 0xFFFF || entries.size() != 4) {
        printf("[mm-text-mods] FAIL: leg 3: the table reads %zu entries up to its terminator, expected 0x0001, "
               "0x0002, 0x0003, 0xFFFF\n",
               entries.size());
        failures++;
    }

    // 4. The rejected overrides changed nothing.
    if (!entries.empty() && entries.back().textId == 0xFFFF &&
        TmBody(entries.back()) != std::string(1, (char)MESSAGE_END)) {
        printf("[mm-text-mods] FAIL: leg 4: the 0xFFFF terminator was overridden\n");
        failures++;
    }
    if (TmFind(entries, 0x0004) != nullptr || TmFind(entries, 0x0005) != nullptr) {
        printf("[mm-text-mods] FAIL: leg 4: another game's text file or an over-long message reached the table\n");
        failures++;
    }

    for (const auto& entry : entries) {
        free((void*)entry.segment);
    }
    free(table);
    restore();

    printf("[mm-text-mods] %s: %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}

#endif
