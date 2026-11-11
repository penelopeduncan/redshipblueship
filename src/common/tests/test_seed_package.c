/**
 * @file test_seed_package.c
 * @brief ROM-free locks for the seed package (#1044, src/common/seed_package.h).
 *
 * Two rows, both display-free (label "redship"):
 *
 *   SeedPackageContainer ("seed-package-container"): the container itself. Entries
 *     written are read back byte for byte (binary entries with NUL bytes and an
 *     empty entry included); an absent entry reads as MISSING_ENTRY, a file that is
 *     not a zip as NOT_A_PACKAGE, a path that does not exist as IO; writing the
 *     same entries twice yields the same archive bytes; the file name is
 *     "<seed name>-<FFFFFFFF>.rsbs-seed.zip"; the generator check accepts this
 *     build's RSBS_GENERATOR_VERSION and nothing else.
 *
 *   SeedPackageGeneratorVersionRefused ("seed-package-generator-refused"): the
 *     production load (OoT_SeedPackage_LoadFile) REFUSES a package whose identity
 *     names another generator version, with RSBS_SEEDPKG_ERR_GENERATOR_VERSION,
 *     before anything is applied: nothing is staged and the resident identity is
 *     untouched. Non-vacuity: the same package at this build's version is refused
 *     for ANOTHER reason (its placement entries are absent), so it was the version
 *     check that refused the first one. RED with the check removed: the bumped
 *     package then reaches the entry check and reports MISSING_ENTRY.
 *
 *     Save format 6 (package format 2) legs, each through the same production load
 *     and each leaving nothing staged and the resident identity untouched:
 *       G4  a v0.5.0-alpha package (generator 4, format 1) reads "Seed package from
 *           another generator" (catches the older-format check moved ahead of the
 *           generator check, which would tell that player "made by another build");
 *       F1  format 1 at this generator is LAYOUT, "Seed package made by another
 *           build" (catches the `format < RSBS_SEED_PACKAGE_FORMAT` refusal removed:
 *           the package would reach the entry check, MISSING_ENTRY);
 *       F3  format 3 is FORMAT, "Seed package from a newer build";
 *       W   a watermark one MM check above this build's is FORMAT before any entry
 *           is read (catches the watermark check removed or comparing the wrong
 *           way: MISSING_ENTRY); W0 a package with no watermark is IDENTITY.
 *     The non-vacuity package carries this build's own watermark, so W's refusal
 *     is the comparison's, not the field's absence.
 *
 *   SeedPackageContainer also locks the format-2 combo-record rule
 *   (OoT_SeedPackage_CheckComboRecordForTest, the production
 *   ComboRecordFromPackageBytes): a v5-era 2048-byte record, a sizeof(ComboContext)
 *   record and a full RSBS_COMBO_CONTEXT_RECORD_SIZE record are accepted (catches
 *   the old exact-size test), a nonzero byte past sizeof is LAYOUT (catches the
 *   tail check removed), a record past the record size or an empty one is LAYOUT
 *   (catches an unbounded copy), and LAYOUT's words are "Seed package made by
 *   another build" (catches the case falling back to "damaged or incomplete").
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++). Its files go to
 * this process's own scratch directory (test_scratch.h).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "../context.h"
#include "../foreign_items.h" // RSBS_GENERATOR_VERSION
#include "../save.h"          // RsbsSave_BuildWatermark: the identity fixtures' watermark (package format 2)
#include "../seed_package.h"
#include "../test_scratch.h"

// games/oot/soh/Enhancements/randomizer/SeedPackageSingleExe.cpp: the production
// combo-record acceptance rule (package format 2).
extern "C" int OoT_SeedPackage_CheckComboRecordForTest(const uint8_t* bytes, size_t size);

namespace {

#define SPK_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

std::string SpkReadFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/** Read @p name back from @p path as a string; "<status>" text when the read refused. */
int SpkReadEntry(const std::string& path, const char* name, std::string& out) {
    void* data = nullptr;
    size_t size = 0;
    const int rc = SeedPackage_ReadEntry(path.c_str(), name, &data, &size);
    out.clear();
    if (rc == RSBS_SEEDPKG_OK && data != nullptr) {
        out.assign(static_cast<const char*>(data), size);
    }
    free(data);
    return rc;
}

/**
 * A package holding only identity.json, at @p generatorVersion and @p format, with
 * this build's save watermark (package format 2) raised by @p mmRcBump MM checks,
 * or no watermark at all when @p withWatermark is false.
 */
int SpkWriteIdentityOnlyEx(const std::string& path, uint32_t generatorVersion, uint32_t format, bool withWatermark,
                           unsigned mmRcBump) {
    ComboSaveWatermark mark;
    RsbsSave_BuildWatermark(&mark);
    char watermark[384] = "";
    if (withWatermark) {
        snprintf(watermark, sizeof(watermark),
                 ",\n  \"watermark\": {\"layoutRevision\": %u, \"ootRcMax\": %u, \"ootRandInfMax\": %u, "
                 "\"ootRskMax\": %u, \"mmRcMax\": %u, \"mmRoMax\": %u, \"mmRtMax\": %u, \"mmRandoInfMax\": %u, "
                 "\"mmReMax\": %u}",
                 (unsigned)mark.layoutRevision, (unsigned)mark.ootRcMax, (unsigned)mark.ootRandInfMax,
                 (unsigned)mark.ootRskMax, (unsigned)mark.mmRcMax + mmRcBump, (unsigned)mark.mmRoMax,
                 (unsigned)mark.mmRtMax, (unsigned)mark.mmRandoInfMax, (unsigned)mark.mmReMax);
    }
    char identity[1024];
    snprintf(identity, sizeof(identity),
             "{\n  \"packageFormat\": %u,\n  \"generatorVersion\": %u%s,\n  \"seedString\": \"RSBSPKGFIXTURE\",\n"
             "  \"seedName\": \"01-02-03-04-05\",\n  \"comboSettingsHash\": \"63F09A31\"\n}\n",
             (unsigned)format, (unsigned)generatorVersion, watermark);
    const char* names[] = { RSBS_SEEDPKG_ENTRY_IDENTITY };
    const void* data[] = { identity };
    const size_t sizes[] = { strlen(identity) };
    return SeedPackage_WriteEntries(path.c_str(), names, data, sizes, 1);
}

/** A package holding only identity.json, at @p generatorVersion, this build's
 *  package format and this build's watermark. */
int SpkWriteIdentityOnly(const std::string& path, uint32_t generatorVersion) {
    return SpkWriteIdentityOnlyEx(path, generatorVersion, RSBS_SEED_PACKAGE_FORMAT, true, 0u);
}

} // namespace

/** SeedPackageContainer: see the file header. */
int SeedPackageContainer_Run(void) {
    int failures = 0;
    const std::filesystem::path dir = rsbs::test::ScratchDir("rsbs-seed-package-container");
    const std::string a = (dir / "a.rsbs-seed.zip").string();
    const std::string b = (dir / "b.rsbs-seed.zip").string();

    static const char kBinary[] = { 'M', 'M', '\0', '\x01', '\xFF', '\0' };
    const std::string identity = "{\"packageFormat\":1,\"generatorVersion\":1}\n";
    const std::string text = "seed=4FF39390\nsettingsHash=AA535593\n";
    const char* names[] = { RSBS_SEEDPKG_ENTRY_IDENTITY, RSBS_SEEDPKG_ENTRY_MM_HALF, RSBS_SEEDPKG_ENTRY_DIGEST,
                            RSBS_SEEDPKG_ENTRY_COMBO_SPOILER };
    const void* data[] = { identity.data(), kBinary, text.data(), "" };
    const size_t sizes[] = { identity.size(), sizeof(kBinary), text.size(), 0 };

    const int writeA = SeedPackage_WriteEntries(a.c_str(), names, data, sizes, 4);
    SPK_ASSERT(writeA == RSBS_SEEDPKG_OK, "writing a four-entry package returned %d (%s)", writeA,
               SeedPackage_StatusName(writeA));
    const int writeB = SeedPackage_WriteEntries(b.c_str(), names, data, sizes, 4);
    SPK_ASSERT(writeB == RSBS_SEEDPKG_OK, "writing it again returned %d", writeB);

    // Every entry back, byte for byte, the binary and the empty one included.
    for (int i = 0; i < 4; i++) {
        std::string got;
        const int rc = SpkReadEntry(a, names[i], got);
        SPK_ASSERT(rc == RSBS_SEEDPKG_OK, "reading '%s' back returned %d (%s)", names[i], rc,
                   SeedPackage_StatusName(rc));
        SPK_ASSERT(got.size() == sizes[i] && memcmp(got.data(), data[i], sizes[i]) == 0,
                   "'%s' read back as %zu byte(s), not the %zu written", names[i], got.size(), sizes[i]);
    }

    // The same entries make the same archive: a package's bytes are a function of
    // its world, not of the moment it was written.
    const std::string bytesA = SpkReadFileBytes(a);
    const std::string bytesB = SpkReadFileBytes(b);
    SPK_ASSERT(!bytesA.empty() && bytesA == bytesB, "two writes of the same entries differ (%zu vs %zu bytes)",
               bytesA.size(), bytesB.size());
    SPK_ASSERT(bytesA.size() >= 4 && bytesA.compare(0, 2, "PK") == 0, "the package is not a zip archive");

    // The refusals name their cause.
    std::string got;
    int rc = SpkReadEntry(a, RSBS_SEEDPKG_ENTRY_OOT_WORLD, got);
    SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_MISSING_ENTRY, "an absent entry read as %d (%s), not missingEntry", rc,
               SeedPackage_StatusName(rc));
    const std::string notZip = (dir / "not-a-zip.rsbs-seed.zip").string();
    {
        std::ofstream out(notZip, std::ios::binary);
        out << "this is a spoiler, not a package\n";
    }
    rc = SpkReadEntry(notZip, RSBS_SEEDPKG_ENTRY_IDENTITY, got);
    SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_NOT_A_PACKAGE, "a text file read as %d (%s), not notAPackage", rc,
               SeedPackage_StatusName(rc));
    rc = SpkReadEntry((dir / "absent.rsbs-seed.zip").string(), RSBS_SEEDPKG_ENTRY_IDENTITY, got);
    SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_IO, "a missing file read as %d (%s), not io", rc, SeedPackage_StatusName(rc));

    // The name.
    char name[96];
    const size_t len = SeedPackage_FileName("01-02-03-04-05", 0x63F09A31u, name, sizeof(name));
    SPK_ASSERT(len > 0 && strcmp(name, "01-02-03-04-05-63F09A31.rsbs-seed.zip") == 0, "the file name is '%s'",
               len > 0 ? name : "");
    char tiny[8];
    SPK_ASSERT(SeedPackage_FileName("01-02-03-04-05", 1u, tiny, sizeof(tiny)) == 0,
               "a too-small buffer still reported a name");

    // The generator check: this build's version and nothing else.
    SPK_ASSERT(SeedPackage_CheckGeneratorVersion(RSBS_GENERATOR_VERSION) == RSBS_SEEDPKG_OK,
               "this build's generator version %u is refused", (unsigned)RSBS_GENERATOR_VERSION);
    SPK_ASSERT(SeedPackage_CheckGeneratorVersion(RSBS_GENERATOR_VERSION + 1u) == RSBS_SEEDPKG_ERR_GENERATOR_VERSION,
               "a newer generator version is accepted");
    SPK_ASSERT(SeedPackage_CheckGeneratorVersion(0u) == RSBS_SEEDPKG_ERR_GENERATOR_VERSION,
               "generator version 0 (no package was ever written by it) is accepted");
    SPK_ASSERT(strcmp(SeedPackage_StatusName(RSBS_SEEDPKG_ERR_GENERATOR_VERSION), "generatorVersion") == 0,
               "the generator refusal is named '%s'", SeedPackage_StatusName(RSBS_SEEDPKG_ERR_GENERATOR_VERSION));
    SPK_ASSERT(SeedPackage_Fnv1a("", 0) == 0x811C9DC5u, "FNV-1a of nothing is %08X",
               (unsigned)SeedPackage_Fnv1a("", 0));

    // Package format 2 (save format 6): the combo record's acceptance rule.
    {
        std::string record(RSBS_COMBO_CONTEXT_RECORD_SIZE + 1u, '\0');
        memcpy(&record[0], COMBO_CONTEXT_MAGIC, 8);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(record.data());
        const size_t kAccepted[] = { 2048u, sizeof(ComboContext), RSBS_COMBO_CONTEXT_RECORD_SIZE };
        for (const size_t size : kAccepted) {
            rc = OoT_SeedPackage_CheckComboRecordForTest(bytes, size);
            printf("[TEST] seed-package-container: a %zu-byte combo record reads %d (%s)\n", size, rc,
                   SeedPackage_StatusName(rc));
            SPK_ASSERT(rc == RSBS_SEEDPKG_OK, "a %zu-byte zero-padded combo record is refused (%d, %s)", size, rc,
                       SeedPackage_StatusName(rc));
        }
        rc = OoT_SeedPackage_CheckComboRecordForTest(bytes, RSBS_COMBO_CONTEXT_RECORD_SIZE + 1u);
        SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_LAYOUT, "a combo record past %u bytes reads %d (%s), not layout",
                   (unsigned)RSBS_COMBO_CONTEXT_RECORD_SIZE, rc, SeedPackage_StatusName(rc));
        rc = OoT_SeedPackage_CheckComboRecordForTest(bytes, 0u);
        SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_LAYOUT, "an empty combo record reads %d (%s), not layout", rc,
                   SeedPackage_StatusName(rc));
        record[sizeof(ComboContext)] = '\x01';
        rc = OoT_SeedPackage_CheckComboRecordForTest(bytes, RSBS_COMBO_CONTEXT_RECORD_SIZE);
        SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_LAYOUT,
                   "a combo record with a byte past this build's %zu reads %d (%s), not layout", sizeof(ComboContext),
                   rc, SeedPackage_StatusName(rc));
        SPK_ASSERT(strcmp(SeedPackage_RefusalWords(RSBS_SEEDPKG_ERR_LAYOUT), "Seed package made by another build") == 0,
                   "a layout refusal reads \"%s\"", SeedPackage_RefusalWords(RSBS_SEEDPKG_ERR_LAYOUT));
    }

    printf("[TEST] seed-package-container: %d failure(s)\n", failures);
    return failures;
}

/** SeedPackageGeneratorVersionRefused: see the file header. */
int SeedPackageGeneratorRefused_Run(void) {
    int failures = 0;
    const std::filesystem::path dir = rsbs::test::ScratchDir("rsbs-seed-package-generator");
    const std::string newer = (dir / "newer.rsbs-seed.zip").string();
    const std::string same = (dir / "same.rsbs-seed.zip").string();

    OoT_SeedPackage_Unstage();
    // A resident identity the refusal must leave exactly as it found it.
    const ComboContext before = gComboCtx;
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x5EED5EEDu;
    gComboCtx.comboSettingsHash = 0x0BADF00Du;
    const ComboContext resident = gComboCtx;

    int rc = SpkWriteIdentityOnly(newer, RSBS_GENERATOR_VERSION + 1u);
    SPK_ASSERT(rc == RSBS_SEEDPKG_OK, "writing the newer-generator fixture returned %d (%s)", rc,
               SeedPackage_StatusName(rc));
    rc = OoT_SeedPackage_LoadFile(newer.c_str());
    SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_GENERATOR_VERSION,
               "a package from generator %u loaded with %d (%s), not generatorVersion",
               (unsigned)RSBS_GENERATOR_VERSION + 1u, rc, SeedPackage_StatusName(rc));
    SPK_ASSERT(OoT_SeedPackage_Staged() == 0, "the refused package is staged");
    SPK_ASSERT(OoT_SeedPackage_LastStatus() == RSBS_SEEDPKG_ERR_GENERATOR_VERSION,
               "the last status after the refusal is %d", OoT_SeedPackage_LastStatus());
    SPK_ASSERT(memcmp(&gComboCtx, &resident, sizeof(ComboContext)) == 0,
               "the refusal changed the resident identity (seed %08X, fingerprint %08X)",
               (unsigned)gComboCtx.sharedRandoSeed, (unsigned)gComboCtx.comboSettingsHash);

    // The legacy label: no package was ever written under generator 0.
    const std::string legacy = (dir / "legacy.rsbs-seed.zip").string();
    rc = SpkWriteIdentityOnly(legacy, 0u);
    SPK_ASSERT(rc == RSBS_SEEDPKG_OK, "writing the generator-0 fixture returned %d", rc);
    rc = OoT_SeedPackage_LoadFile(legacy.c_str());
    SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_GENERATOR_VERSION, "a generator-0 package loaded with %d (%s)", rc,
               SeedPackage_StatusName(rc));

    // Non-vacuity: at this build's version the same package gets past the version
    // check and is refused for its absent placement entries instead.
    rc = SpkWriteIdentityOnly(same, RSBS_GENERATOR_VERSION);
    SPK_ASSERT(rc == RSBS_SEEDPKG_OK, "writing the same-generator fixture returned %d", rc);
    rc = OoT_SeedPackage_LoadFile(same.c_str());
    SPK_ASSERT(rc == RSBS_SEEDPKG_ERR_MISSING_ENTRY,
               "the same package at this build's generator loaded with %d (%s), not missingEntry: the version check "
               "is not what refused the newer one",
               rc, SeedPackage_StatusName(rc));
    SPK_ASSERT(OoT_SeedPackage_Staged() == 0, "an incomplete package is staged");
    SPK_ASSERT(memcmp(&gComboCtx, &resident, sizeof(ComboContext)) == 0,
               "the incomplete package changed the resident identity");

    // Save format 6 (package format 2): G4, F1, F3, W, W0 (the file header).
    struct SpkFormatLeg {
        const char* name;
        uint32_t generator;
        uint32_t format;
        bool withWatermark;
        unsigned mmRcBump;
        int expected;
        const char* words;
    };
    const SpkFormatLeg kLegs[] = {
        { "G4", 4u, 1u, false, 0u, RSBS_SEEDPKG_ERR_GENERATOR_VERSION, "Seed package from another generator" },
        { "F1", RSBS_GENERATOR_VERSION, 1u, true, 0u, RSBS_SEEDPKG_ERR_LAYOUT, "Seed package made by another build" },
        { "F3", RSBS_GENERATOR_VERSION, RSBS_SEED_PACKAGE_FORMAT + 1u, true, 0u, RSBS_SEEDPKG_ERR_FORMAT,
          "Seed package from a newer build" },
        { "W", RSBS_GENERATOR_VERSION, RSBS_SEED_PACKAGE_FORMAT, true, 1u, RSBS_SEEDPKG_ERR_FORMAT,
          "Seed package from a newer build" },
        { "W0", RSBS_GENERATOR_VERSION, RSBS_SEED_PACKAGE_FORMAT, false, 0u, RSBS_SEEDPKG_ERR_IDENTITY,
          "Seed package damaged or incomplete" },
    };
    for (const SpkFormatLeg& leg : kLegs) {
        const std::string path = (dir / (std::string(leg.name) + ".rsbs-seed.zip")).string();
        rc = SpkWriteIdentityOnlyEx(path, leg.generator, leg.format, leg.withWatermark, leg.mmRcBump);
        SPK_ASSERT(rc == RSBS_SEEDPKG_OK, "%s: writing the fixture returned %d", leg.name, rc);
        rc = OoT_SeedPackage_LoadFile(path.c_str());
        printf("[TEST] seed-package-generator-refused: %s (generator %u, format %u, watermark %s) loaded with %d "
               "(%s): \"%s\"\n",
               leg.name, (unsigned)leg.generator, (unsigned)leg.format,
               leg.withWatermark ? (leg.mmRcBump != 0 ? "above this build's" : "this build's") : "none", rc,
               SeedPackage_StatusName(rc), SeedPackage_RefusalWords(rc));
        SPK_ASSERT(rc == leg.expected, "%s loaded with %d (%s), not %d (%s)", leg.name, rc, SeedPackage_StatusName(rc),
                   leg.expected, SeedPackage_StatusName(leg.expected));
        SPK_ASSERT(strcmp(SeedPackage_RefusalWords(rc), leg.words) == 0, "%s reads \"%s\", not \"%s\"", leg.name,
                   SeedPackage_RefusalWords(rc), leg.words);
        SPK_ASSERT(OoT_SeedPackage_Staged() == 0, "%s: the refused package is staged", leg.name);
        SPK_ASSERT(memcmp(&gComboCtx, &resident, sizeof(ComboContext)) == 0,
                   "%s: the refusal changed the resident identity", leg.name);
    }

    gComboCtx = before;
    printf("[TEST] seed-package-generator-refused: %d failure(s)\n", failures);
    return failures;
}
