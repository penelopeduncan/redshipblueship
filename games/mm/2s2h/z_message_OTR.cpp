#include "BenPort.h"
#include <ship/resource/ResourceManager.h>
#include "2s2h/resource/type/Scene.h"
#include <ship/utils/StringHelper.h>
#include "2s2h/resource/type/TextMM.h"
#include <message_data_static.h>
#include <spdlog/spdlog.h>
#include <algorithm>

extern "C" MessageTableEntry* sMessageTableNES;
extern "C" MessageTableEntry* sMessageTableCredits;

static MessageTableEntry* BuildTable(const std::vector<MessageTableEntry>& entries) {
    // Allocate room for an additional message
    // OTRTODO: Should not be malloc'ing here. It's fine for now since we check elsewhere that the message table is
    // already null.
    MessageTableEntry* table = (MessageTableEntry*)malloc(sizeof(MessageTableEntry) * (entries.size() + 1));

    std::copy(entries.begin(), entries.end(), table);

    return table;
}

static MessageTableEntry BuildEntry(const S2H::MessageEntryMM& msg) {
    MessageTableEntry entry;
    entry.textId = msg.id;
    entry.typePos = (msg.textboxType << 4) | msg.textboxYPos;
    entry.msgSize = msg.msg.size() + S2H::TextMM::HeaderSize;

    auto segment = (char*)malloc(entry.msgSize);

    segment[0] = msg.textboxType;
    segment[1] = msg.textboxYPos;
    segment[2] = msg.icon;
    segment[3] = (msg.nextMessageID & 0xFF00) >> 8;
    segment[4] = (msg.nextMessageID & 0x00FF);
    segment[5] = (msg.firstItemCost & 0xFF00) >> 8;
    segment[6] = (msg.firstItemCost & 0x00FF);
    segment[7] = (msg.secondItemCost & 0xFF00) >> 8;
    segment[8] = (msg.secondItemCost & 0x00FF);
    segment[9] = 0xFF;
    segment[10] = 0xFF;

    memcpy(&segment[S2H::TextMM::HeaderSize], msg.msg.c_str(), msg.msg.size());

    entry.segment = segment;
    return entry;
}

static std::vector<MessageTableEntry> BuildEntries(const std::vector<S2H::MessageEntryMM>& messages) {
    std::vector<MessageTableEntry> entries;
    entries.reserve(messages.size());

    for (auto& msg : messages) {
        entries.push_back(BuildEntry(msg));
    }

    return entries;
}

static void UpsertEntry(std::vector<MessageTableEntry>& entries, const S2H::MessageEntryMM& msg) {
    if (msg.id == 0xFFFF) {
        SPDLOG_WARN("Message override 0xFFFF ends the table and can't be overridden");
        return;
    }

    if (msg.msg.size() > S2H::TextMM::MaxMessageSize) {
        SPDLOG_WARN("Message override 0x{:04X} is {} bytes, over the {} byte limit", msg.id, msg.msg.size(),
                    S2H::TextMM::MaxMessageSize);
        return;
    }

    // Lookups stop at the 0xFFFF entry, if the table has one
    auto end = std::find_if(entries.begin(), entries.end(),
                            [](const MessageTableEntry& entry) { return entry.textId == 0xFFFF; });
    auto existing =
        std::find_if(entries.begin(), end, [&](const MessageTableEntry& entry) { return entry.textId == msg.id; });

    if (existing == end) {
        entries.insert(end, BuildEntry(msg));
    } else {
        free((void*)existing->segment);
        *existing = BuildEntry(msg);
    }
}

static std::vector<std::string> ListOverrideFiles(const std::string& folder) {
    auto archiveManager = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
    std::vector<std::string> files;

    // A .meta alias is loaded by its path without the extension
    auto paths = archiveManager->ListFiles(folder + "/*");
    for (auto& path : *paths) {
        files.push_back(path.ends_with(".meta") ? path.substr(0, path.size() - 5) : path);
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());

    return files;
}

// Upstream asks libultraship's ArchiveManager::GetFilePriority (Kenix3/libultraship #1218), which this tree's
// fork does not have. Its answer is the mount-order index of the archive that serves the path, -1 when no
// mounted archive does. The fork's mount order is the order of GetArchives(): AddArchive appends, and a re-add
// (every game switch re-adds the arriving game's archives, then its mods) moves the archive to the back, which is
// exactly the newest-wins order lookups follow. So the same rank is read here from the fork's own API.
static int32_t GetFilePriority(const std::string& path) {
    auto archiveManager = Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager();
    auto owner = archiveManager->GetArchiveFromFile(path);
    if (owner == nullptr) {
        return -1;
    }

    auto archives = archiveManager->GetArchives();
    auto found = std::find(archives->begin(), archives->end(), owner);
    return found == archives->end() ? -1 : static_cast<int32_t>(found - archives->begin());
}

static std::vector<std::shared_ptr<S2H::TextMM>> LoadOverrides(const std::string& folder) {
    auto resourceManager = Ship::Context::GetInstance()->GetResourceManager();
    std::vector<std::shared_ptr<S2H::TextMM>> files;

    for (auto& path : ListOverrideFiles(folder)) {
        // dynamic_pointer_cast: the folder is MM's, but a file another game's mod put there would load as that
        // game's resource type (OoT's SOH::Text has a different entry layout), and is skipped here.
        auto file = std::dynamic_pointer_cast<S2H::TextMM>(resourceManager->LoadResource(path));

        if (file == nullptr) {
            SPDLOG_ERROR("Failed to load message override {}", path);
            continue;
        }

        files.push_back(file);
    }

    // Apply lowest-to-highest resolved archive priority so higher priority wins; keep path order for ties.
    std::stable_sort(files.begin(), files.end(), [&](const auto& a, const auto& b) {
        return GetFilePriority(a->GetInitData()->Path) < GetFilePriority(b->GetInitData()->Path);
    });

    return files;
}

static void ApplyOverrides(std::vector<MessageTableEntry>& entries, const std::string& folder) {
    for (auto& file : LoadOverrides(folder)) {
        for (auto& msg : file->messages) {
            UpsertEntry(entries, msg);
        }
    }
}

// static: OoT's z_message_OTR.cpp has its own OTRMessage_LoadTable (single-exe, #344)
static MessageTableEntry* OTRMessage_LoadTable(const char* filePath, bool isNES) {
    auto file = std::static_pointer_cast<S2H::TextMM>(
        Ship::Context::GetInstance()->GetResourceManager()->LoadResource(filePath));

    if (file == nullptr)
        return nullptr;

    // Text mods (2Ship #2000): every TextMM in override/<the table's folder>/ replaces or adds entries. MM's tables
    // live under text/message_data_static/, a folder OoT never reads (OoT's are text/{nes,ger,fra,jpn}_message_data_
    // static/ and text/staff_message_data_static/, and MM's staff table takes no overrides), so an MM text mod and
    // an OoT one cannot reach each other's tables.
    std::string path = filePath;
    std::vector<MessageTableEntry> entries = BuildEntries(file->messages);
    ApplyOverrides(entries, "override/" + path.substr(0, path.find_last_of('/')));

    return BuildTable(entries);
}

// The ROM-free MMTextMods row (games/mm/2s2h/mm_text_mods_test.cpp) builds a table from its own scratch archives
// through the same loader MM_OTRMessage_Init uses for the real one.
extern "C" MessageTableEntry* MM_OTRMessage_LoadTableHeadless(const char* filePath) {
    return OTRMessage_LoadTable(filePath, false);
}

// MM_-prefixed: OoT's z_message_OTR.cpp owns the extern "C" OTRMessage_Init symbol (single-exe, #344).
// Idempotent so MM_Game_Init can call it on every (re-)entry without leaking tables.
extern "C" void MM_OTRMessage_Init() {
    if (sMessageTableNES != NULL) {
        return;
    }

    sMessageTableNES = OTRMessage_LoadTable("text/message_data_static/message_data_static", true);
    if (sMessageTableNES == NULL) {
        // Every MM textbox (including scene title cards fired right after a
        // scene load, #344) dereferences this table — make the root cause
        // visible instead of crashing later in MM_Message_FindMessage.
        fprintf(stderr, "[MM] FATAL: failed to load message table "
                        "'text/message_data_static/message_data_static' — mm.o2r is incomplete\n");
        fflush(stderr);
        return;
    }

    auto file2 = std::static_pointer_cast<S2H::TextMM>(Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        "text/staff_message_data_static/staff_message_data_static"));
    if (file2 == nullptr) {
        fprintf(stderr, "[MM] WARNING: failed to load credits message table "
                        "'text/staff_message_data_static/staff_message_data_static'\n");
        fflush(stderr);
        return;
    }
    sMessageTableCredits = (MessageTableEntry*)malloc(sizeof(MessageTableEntry) * file2->messages.size());

    for (size_t i = 0; i < file2->messages.size(); i++) {
        sMessageTableCredits[i].textId = file2->messages[i].id;
        sMessageTableCredits[i].typePos = (file2->messages[i].textboxType << 4) | file2->messages[i].textboxYPos;
        sMessageTableCredits[i].segment = file2->messages[i].msg.c_str();
        sMessageTableCredits[i].msgSize = file2->messages[i].msg.size();
    }
}
