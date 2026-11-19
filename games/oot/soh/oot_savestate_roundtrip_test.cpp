/**
 * @file oot_savestate_roundtrip_test.cpp
 * ROM-free, display-free lock for backport packet #1752 (and #1735 savestates
 * half): Barinade / Bongo Bongo / Morpha statics (PR #6914), OoT_sEffectContext
 * (PR #7042), z_player statics (PR #6927), AudioOcarina statics (PR #6954), and
 * transition actor ID save/restore (PR #6838). CTest label "redship", row
 * OoTSaveStateRoundtrip in CMake/SingleExecutable.cmake, dispatch
 * "oot-savestate-roundtrip" in src/common/test_runner.cpp.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdio>
#include <cstring>
#include <memory>

#include <z64.h>
#include "macros.h"

namespace {

struct BossVa;

typedef struct BossVaEffect {
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;
    u8 type;
    u16 timer;
    s16 mode;
    Vec3s rot;
    s16 primColor[4];
    s16 envColor[4];
    f32 scale;
    f32 scaleMod;
    Vec3f offset;
    struct BossVa* parent;
    u32 epoch;
} BossVaEffect;

typedef struct {
    Vec3f pos;
    Vec3f vel;
    Vec3f accel;
    u8 type;
    u8 timer;
    u8 stopTimer;
    s16 unk_28;
    s16 alpha;
    s16 rippleMode;
    s16 maxAlpha;
    f32 scale;
    f32 fwork[2];
    Vec3f* targetPos;
    u32 epoch;
} BossMoEffect;

typedef struct {
    f32 vol;
    f32 freqScale;
    s8 reverb;
    s8 panSigned;
    s8 stereoBits;
    u8 filter;
    u8 unk_0C;
} SfxPlayerState;

typedef struct {
    f32 value;
    f32 target;
    f32 step;
    s32 remainingFrames;
} FreqLerp;

typedef struct {
    s8 x;
    s8 y;
} OcarinaStick;

} // namespace

extern "C" {
void* OoT_SaveState_TestCreate(void);
void OoT_SaveState_TestDestroy(void* state);
void OoT_SaveState_TestSavePlainBlocks(void* state);
void OoT_SaveState_TestLoadPlainBlocks(void* state);

extern PlayState* OoT_gPlayState;
extern EffectContext OoT_sEffectContext;

// PR #6914: z_boss_mo
extern void* sMorphaCore;
extern void* sMorphaTent1;
extern void* sMorphaTent2;
extern BossMoEffect OoT_sEffects[300];
extern s32 sBossMoSeed1;
extern s32 sBossMoSeed2;
extern s32 sBossMoSeed3;

// PR #6914: z_boss_sst
extern void* sHead;
extern void* sHands[2];
extern void* sFloor;
extern Vec3f sHandOffsets[2];
extern s16 sHandYawOffsets[2];
extern s16 sCutsceneCamera;
extern u32 sBodyStatic;
extern Color_RGBA8 sBodyColor;
extern Color_RGBA8 sStaticColor;

// PR #6914: z_boss_va
extern u8 sKillBari;
extern u8 sBodyBari[10];
extern s16 sCsCamera;
extern BossVaEffect sVaEffects[400];
extern u8 sBodyState;
extern u8 sFightPhase;
extern s8 sCsState;
extern s16 sDoorState;
extern u8 sPhase3StopMoving;
extern Vec3s sZapperRot;
extern u16 sPhase2Timer;
extern s8 sPhase4HP;

// PR #6927: z_player
extern s32 D_80858AA0;
extern s32 OoT_sSavedCurrentMask;
extern Vec3f OoT_sInteractWallCheckResult;
extern Input* sControlInput;
extern u32 OoT_sNoclipEnabled;
extern f32 OoT_sControlStickMagnitude;
extern s16 OoT_sControlStickAngle;
extern s16 OoT_sControlStickWorldYaw;
extern s32 OoT_sUpperBodyIsBusy;
extern s32 sFloorType;
extern f32 OoT_sWaterSpeedFactor;
extern f32 OoT_sInvWaterSpeedFactor;
extern u32 sTouchedWallFlags;
extern u32 sConveyorSpeed;
extern s16 sIsFloorConveyor;
extern s16 sConveyorYaw;
extern f32 sYDistToFloor;
extern s32 OoT_sPrevFloorProperty;
extern s32 OoT_sShapeYawToTouchedWall;
extern s32 OoT_sWorldYawToTouchedWall;
extern s16 sFloorShapePitch;
extern s32 sUseHeldItem;
extern s32 sHeldItemButtonIsHeldDown;

// PR #6954: code_800EC960
extern OcarinaNote sOcarinaSongs[OCARINA_SONG_MAX][20];
extern u8 sOcaMinigameAppendPos;
extern u8 sOcaMinigameEndPos;
extern f32 D_801305B0;
extern s8 D_801305B4;
extern s8 D_801305B8;
extern s8 D_801305BC;
extern s8 D_801305C0;
extern u8 OoT_sAudioIncreasingTranspose;
extern u8 OoT_sPrevChargeLevel;
extern f32 D_801305F4;
extern u8 D_80130600;
extern s8 D_80130608;
extern s8 OoT_sAudioCutsceneFlag;
extern s8 OoT_sSpecReverb;
extern s8 OoT_sAudioEnvReverb;
extern s8 OoT_sAudioCodeReverb;
extern u8 OoT_sPrevSeqMode;
extern f32 sAudioEnemyDist;
extern s8 sAudioEnemyVol;
extern u16 OoT_sPrevMainBgmSeqId;
extern u8 D_8013062C;
extern u8 D_80130630;
extern u32 OoT_sNumFramesStill;
extern u32 OoT_sNumFramesMoving;
extern u8 OoT_sAudioBaseFilter;
extern u8 OoT_sAudioExtraFilter;
extern u8 OoT_sAudioBaseFilter2;
extern u8 OoT_sAudioExtraFilter2;
extern Vec3f* sSariaBgmPtr;
extern f32 D_80130650;
extern f32 D_8016B7A8;
extern f32 D_8016B7AC;
extern f32 D_8016B7B0;
extern f32 D_8016B7B4;
extern FreqLerp OoT_sRiverFreqScaleLerp;
extern FreqLerp OoT_sWaterfallFreqScaleLerp;
extern f32 D_8016B7D8;
extern s8 D_8016B7DC;
extern f32 D_8016B7E0;
extern u8 D_8016B8B0;
extern u8 D_8016B8B1;
extern u8 D_8016B8B2;
extern u8 D_8016B8B3;
extern u8 sAudioGanonDistVol;
extern SfxPlayerState OoT_sSfxChannelState[0x10];
extern u8 D_8016B9D8;
extern u8 D_8016B9F2;
extern u8 D_8016B9F3;
extern u8 D_8016B9F4;
extern u16 D_8016B9F6;
extern u8 OoT_sRecordingState;
extern u8 OoT_sRecordSongPos;
extern u32 D_80131860;
extern u8 D_80131864;
extern u8 D_80131868;
extern u8 D_8013186C;
extern s8 D_80131870;
extern u8 D_80131874;
extern u8 D_80131878;
extern u8 D_8013187C;
extern u8 OoT_sOcarinaDropInputTimer;
extern OcarinaStaff OoT_sPlayingStaff;
extern OcarinaStaff sDisplayedStaff;
extern OcarinaStaff OoT_sRecordingStaff;
extern u8 OoT_sNotePlaybackVolume;
extern u8 OoT_sNotePlaybackVibrato;
extern s8 sNotePlaybackTone;
extern f32 sNormalizedNotePlaybackTone;
extern f32 sNormalizedNotePlaybackVolume;
extern s32 D_80130F68;
extern u16 D_80130F4C;
extern u32 D_8016BA04;
extern OcarinaStick sCurOcaStick;

// Saved before this packet, restored only since the QC pass (LoadMiscCodeData used to copy
// each of these onto itself).
extern u16 sLearnSongPos[0x10];
extern u16 D_8016BA50[0x10];
extern u16 D_8016BA70[0x10];
extern u8 sLearnSongExpectedNote[0x10];
}

namespace {

uint8_t sDummyPtrTargets[16];

struct TestSaveState {
    void* handle = OoT_SaveState_TestCreate();
    TestSaveState() = default;
    ~TestSaveState() {
        if (handle != nullptr) {
            OoT_SaveState_TestDestroy(handle);
        }
    }
    TestSaveState(const TestSaveState&) = delete;
    TestSaveState& operator=(const TestSaveState&) = delete;
    void Save() { OoT_SaveState_TestSavePlainBlocks(handle); }
    void Load() { OoT_SaveState_TestLoadPlainBlocks(handle); }
};

struct BaselineGuard {
    PlayState* prevPlay = OoT_gPlayState;
    TestSaveState baseline;
    BaselineGuard() {
        OoT_gPlayState = nullptr;
        baseline.Save();
    }
    ~BaselineGuard() {
        OoT_gPlayState = nullptr;
        baseline.Load();
        OoT_gPlayState = prevPlay;
    }
};

void PopulateEffectContext(EffectContext& ctx, bool sentinel) {
    std::memset(&ctx, 0, sizeof(ctx));
    const u8 active = sentinel ? 1 : 0;
    const f32 val = sentinel ? 12.5f : -4.25f;
    ctx.sparks[0].status.active = active;
    ctx.sparks[0].effect.speed = val;
    ctx.sparks[SPARK_COUNT - 1].status.active = active;
    ctx.sparks[SPARK_COUNT - 1].effect.gravity = val + 1.0f;
    ctx.blures[0].status.active = active;
    ctx.blures[0].effect.elemDuration = sentinel ? 17 : 3;
    ctx.blures[BLURE_COUNT - 1].status.active = active;
    ctx.blures[BLURE_COUNT - 1].effect.flags = sentinel ? 0xA5A5 : 0x1234;
    ctx.shieldParticles[0].status.active = active;
    ctx.shieldParticles[0].effect.maxInitialSpeed = val + 2.0f;
    ctx.shieldParticles[SHIELD_PARTICLE_COUNT - 1].status.active = active;
    ctx.shieldParticles[SHIELD_PARTICLE_COUNT - 1].effect.numElements = sentinel ? 9 : 1;
}

void SeedBosses(bool sentinel) {
    const size_t pBase = sentinel ? 0 : 8;
    const s32 seedBase = sentinel ? 1000 : -99;
    const f32 fBase = sentinel ? 42.5f : -13.25f;

    sMorphaCore = &sDummyPtrTargets[pBase + 0];
    sMorphaTent1 = &sDummyPtrTargets[pBase + 1];
    sMorphaTent2 = &sDummyPtrTargets[pBase + 2];
    std::memset(OoT_sEffects, 0, sizeof(OoT_sEffects));
    for (int idx : { 0, 299 }) {
        OoT_sEffects[idx].pos = { fBase + idx, fBase * 2.0f, fBase * 3.0f };
        OoT_sEffects[idx].type = static_cast<u8>(sentinel ? (3 + idx % 5) : 9);
        OoT_sEffects[idx].targetPos = reinterpret_cast<Vec3f*>(&sDummyPtrTargets[pBase + 3]);
        OoT_sEffects[idx].epoch = sentinel ? (0xBEEF0000u + idx) : 0x1234u;
    }
    sBossMoSeed1 = seedBase + 11;
    sBossMoSeed2 = seedBase + 22;
    sBossMoSeed3 = seedBase + 33;

    sHead = &sDummyPtrTargets[pBase + 4];
    sHands[0] = &sDummyPtrTargets[pBase + 5];
    sHands[1] = &sDummyPtrTargets[pBase + 6];
    sFloor = &sDummyPtrTargets[pBase + 7];
    sHandOffsets[0] = { fBase, fBase + 1.0f, fBase + 2.0f };
    sHandOffsets[1] = { fBase + 3.0f, fBase + 4.0f, fBase + 5.0f };
    sHandYawOffsets[0] = sentinel ? 1234 : -111;
    sHandYawOffsets[1] = sentinel ? -5678 : 222;
    sCutsceneCamera = sentinel ? 7 : 1;
    sBodyStatic = sentinel ? 1u : 0u;
    sBodyColor = sentinel ? Color_RGBA8{ 11, 22, 33, 44 } : Color_RGBA8{ 200, 201, 202, 203 };
    sStaticColor = sentinel ? Color_RGBA8{ 55, 66, 77, 88 } : Color_RGBA8{ 1, 2, 3, 4 };

    sKillBari = sentinel ? 1 : 0;
    for (int i = 0; i < 10; i++) {
        sBodyBari[i] = static_cast<u8>((sentinel ? 0xA0 : 0x10) + i);
    }
    sCsCamera = sentinel ? 12 : 2;
    std::memset(sVaEffects, 0, sizeof(sVaEffects));
    for (int idx : { 0, 399 }) {
        sVaEffects[idx].pos = { fBase + 10.0f, fBase + 20.0f, fBase + 30.0f };
        sVaEffects[idx].type = static_cast<u8>(sentinel ? 6 : 1);
        sVaEffects[idx].timer = static_cast<u16>(sentinel ? (500 + idx) : 12);
        sVaEffects[idx].parent = reinterpret_cast<BossVa*>(&sDummyPtrTargets[pBase + 0]);
        sVaEffects[idx].epoch = sentinel ? (0xCAFE0000u + idx) : 0x5555u;
    }
    sBodyState = sentinel ? 4 : 0;
    sFightPhase = sentinel ? 3 : 0;
    sCsState = sentinel ? -5 : 1;
    sDoorState = sentinel ? 9 : 0;
    sPhase3StopMoving = sentinel ? 1 : 0;
    sZapperRot = sentinel ? Vec3s{ 100, -200, 300 } : Vec3s{ -1, -2, -3 };
    sPhase2Timer = sentinel ? 4321 : 99;
    sPhase4HP = sentinel ? 8 : -1;
}

void SeedPlayer(bool sentinel) {
    const size_t pBase = sentinel ? 0 : 8;
    const s32 s = sentinel ? 1 : 0;
    const f32 f = sentinel ? 3.5f : -7.5f;

    D_80858AA0 = s ? 0x10203040 : -5;
    OoT_sSavedCurrentMask = s ? 7 : 2;
    OoT_sInteractWallCheckResult = { f, f + 1.0f, f + 2.0f };
    sControlInput = reinterpret_cast<Input*>(&sDummyPtrTargets[pBase + 1]);
    OoT_sNoclipEnabled = s ? 1u : 0u;
    OoT_sControlStickMagnitude = s ? 55.25f : 1.0f;
    OoT_sControlStickAngle = s ? 0x1234 : -10;
    OoT_sControlStickWorldYaw = s ? -0x2345 : 20;
    OoT_sUpperBodyIsBusy = s ? 1 : 0;
    sFloorType = s ? 14 : 3;
    OoT_sWaterSpeedFactor = s ? 0.5f : 2.0f;
    OoT_sInvWaterSpeedFactor = s ? 2.0f : 0.25f;
    sTouchedWallFlags = s ? 0xA5A5A5A5u : 0x1u;
    sConveyorSpeed = s ? 18u : 2u;
    sIsFloorConveyor = s ? 1 : 0;
    sConveyorYaw = s ? 0x4000 : -0x1000;
    sYDistToFloor = s ? -12.75f : 99.5f;
    OoT_sPrevFloorProperty = s ? 9 : 0;
    OoT_sShapeYawToTouchedWall = s ? 0x3000 : -100;
    OoT_sWorldYawToTouchedWall = s ? -0x4000 : 200;
    sFloorShapePitch = s ? 0x0500 : -0x0200;
    sUseHeldItem = s ? 1 : 0;
    sHeldItemButtonIsHeldDown = s ? 1 : 0;
}

void SeedAudioOcarina(bool sentinel) {
    const size_t pBase = sentinel ? 0 : 8;
    const u8 b = sentinel ? 0x40 : 0x05;
    const s8 sb = sentinel ? -27 : 12;
    const f32 f = sentinel ? 1.375f : -8.5f;

    std::memset(sOcarinaSongs[OCARINA_SONG_MEMORY_GAME], 0, sizeof(sOcarinaSongs[OCARINA_SONG_MEMORY_GAME]));
    std::memset(sOcarinaSongs[OCARINA_SONG_SCARECROW], 0, sizeof(sOcarinaSongs[OCARINA_SONG_SCARECROW]));
    for (int i = 0; i < 20; i++) {
        sOcarinaSongs[OCARINA_SONG_MEMORY_GAME][i] = { static_cast<u8>((sentinel ? 1 : 10) + i), b,
                                                       static_cast<u16>(sentinel ? (300 + i) : 7),
                                                       static_cast<u8>(b + 1), static_cast<u8>(b + 2), sb,
                                                       static_cast<u8>(sentinel ? 2 : 0) };
        sOcarinaSongs[OCARINA_SONG_SCARECROW][i] = { static_cast<u8>((sentinel ? 21 : 5) + i), static_cast<u8>(b + 3),
                                                     static_cast<u16>(sentinel ? (600 + i) : 9),
                                                     static_cast<u8>(b + 4), static_cast<u8>(b + 5),
                                                     static_cast<s8>(sb - 1), static_cast<u8>(sentinel ? 3 : 1) };
    }
    sOcaMinigameAppendPos = sentinel ? 11 : 1;
    sOcaMinigameEndPos = sentinel ? 16 : 2;
    D_801305B0 = f;
    D_801305B4 = sb;
    D_801305B8 = static_cast<s8>(sb + 1);
    D_801305BC = static_cast<s8>(sb + 2);
    D_801305C0 = static_cast<s8>(sb + 3);
    OoT_sAudioIncreasingTranspose = sentinel ? 5 : 0;
    OoT_sPrevChargeLevel = sentinel ? 2 : 9;
    D_801305F4 = f + 1.0f;
    D_80130600 = sentinel ? 18 : 1;
    D_80130608 = sentinel ? -3 : 4;
    OoT_sAudioCutsceneFlag = sentinel ? 1 : 0;
    OoT_sSpecReverb = sentinel ? 40 : -5;
    OoT_sAudioEnvReverb = sentinel ? 25 : 2;
    OoT_sAudioCodeReverb = sentinel ? 15 : 3;
    OoT_sPrevSeqMode = sentinel ? 0x82 : 0x01;
    sAudioEnemyDist = f + 100.0f;
    sAudioEnemyVol = sentinel ? 64 : -10;
    OoT_sPrevMainBgmSeqId = sentinel ? 0x0045 : 0x00FF;
    D_8013062C = sentinel ? 7 : 0;
    D_80130630 = sentinel ? 29 : 1;
    OoT_sNumFramesStill = sentinel ? 12345u : 1u;
    OoT_sNumFramesMoving = sentinel ? 67890u : 2u;
    OoT_sAudioBaseFilter = sentinel ? 0x22 : 0x01;
    OoT_sAudioExtraFilter = sentinel ? 0x33 : 0x02;
    OoT_sAudioBaseFilter2 = sentinel ? 0x44 : 0x03;
    OoT_sAudioExtraFilter2 = sentinel ? 0x55 : 0x04;
    sSariaBgmPtr = reinterpret_cast<Vec3f*>(&sDummyPtrTargets[pBase + 2]);
    D_80130650 = f + 200.0f;
    D_8016B7A8 = f + 2.0f;
    D_8016B7AC = f + 3.0f;
    D_8016B7B0 = f + 4.0f;
    D_8016B7B4 = f + 5.0f;
    OoT_sRiverFreqScaleLerp = { f + 0.1f, f + 0.2f, f + 0.3f, sentinel ? 40 : 1 };
    OoT_sWaterfallFreqScaleLerp = { f + 0.4f, f + 0.5f, f + 0.6f, sentinel ? 20 : 2 };
    D_8016B7D8 = f + 6.0f;
    D_8016B7DC = sentinel ? -12 : 12;
    D_8016B7E0 = f + 7.0f;
    D_8016B8B0 = sentinel ? 90 : 10;
    D_8016B8B1 = sentinel ? 91 : 11;
    D_8016B8B2 = sentinel ? 1 : 0;
    D_8016B8B3 = sentinel ? 1 : 0;
    sAudioGanonDistVol = sentinel ? 0x77 : 0x11;
    std::memset(OoT_sSfxChannelState, 0, sizeof(OoT_sSfxChannelState));
    for (int i = 0; i < 0x10; i++) {
        OoT_sSfxChannelState[i] = { f + i, f + 0.5f + i, static_cast<s8>(sb + i), static_cast<s8>(i - 8),
                                    static_cast<s8>(sentinel ? 3 : 0), static_cast<u8>(b + i),
                                    static_cast<u8>(sentinel ? 1 : 0) };
    }
    D_8016B9D8 = sentinel ? 9 : 1;
    D_8016B9F2 = sentinel ? 1 : 0;
    D_8016B9F3 = sentinel ? 4 : 0;
    D_8016B9F4 = sentinel ? 5 : 0;
    D_8016B9F6 = sentinel ? 0x1234 : 0x0001;
    OoT_sRecordingState = sentinel ? 2 : 0;
    OoT_sRecordSongPos = sentinel ? 14 : 0;
    D_80131860 = sentinel ? 0x89ABCDEFu : 1u;
    D_80131864 = sentinel ? 0x18 : 0x01;
    D_80131868 = sentinel ? 0x7F : 0x02;
    D_8013186C = sentinel ? 0x0A : 0x03;
    D_80131870 = sentinel ? -6 : 5;
    D_80131874 = sentinel ? 4 : 0;
    D_80131878 = sentinel ? 13 : 0;
    D_8013187C = sentinel ? 8 : 1;
    OoT_sOcarinaDropInputTimer = sentinel ? 3 : 0;
    OoT_sPlayingStaff = { static_cast<u8>(sentinel ? 5 : 0), static_cast<u8>(sentinel ? 2 : 9),
                          static_cast<u8>(sentinel ? 6 : 1) };
    sDisplayedStaff = { static_cast<u8>(sentinel ? 7 : 1), static_cast<u8>(sentinel ? 3 : 8),
                        static_cast<u8>(sentinel ? 4 : 2) };
    OoT_sRecordingStaff = { static_cast<u8>(sentinel ? 9 : 2), static_cast<u8>(sentinel ? 1 : 7),
                            static_cast<u8>(sentinel ? 12 : 3) };
    OoT_sNotePlaybackVolume = sentinel ? 0x60 : 0x01;
    OoT_sNotePlaybackVibrato = sentinel ? 0x12 : 0x02;
    sNotePlaybackTone = sentinel ? -15 : 4;
    sNormalizedNotePlaybackTone = f + 8.0f;
    sNormalizedNotePlaybackVolume = f + 9.0f;
    D_80130F68 = sentinel ? 987654 : -1;
    D_80130F4C = sentinel ? 0xC001 : 0x0002;
    D_8016BA04 = sentinel ? 0xFEDCBA98u : 3u;
    sCurOcaStick = { static_cast<s8>(sentinel ? 50 : -5), static_cast<s8>(sentinel ? -60 : 6) };
    for (int i = 0; i < 0x10; i++) {
        sLearnSongPos[i] = static_cast<u16>((sentinel ? 0x100 : 0x10) + i);
        D_8016BA50[i] = static_cast<u16>((sentinel ? 0x200 : 0x20) + i);
        D_8016BA70[i] = static_cast<u16>((sentinel ? 0x300 : 0x30) + i);
        sLearnSongExpectedNote[i] = static_cast<u8>((sentinel ? 0x40 : 0x04) + i);
    }
}

#define CHECK(cond, msg)                                                                  \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::printf("[TEST] FAIL (oot-savestate-roundtrip): %s\n", (msg));            \
            return 1;                                                                     \
        }                                                                                 \
    } while (0)

int CheckVec3fEq(const Vec3f& a, const Vec3f& b, const char* name) {
    if (a.x != b.x || a.y != b.y || a.z != b.z) {
        std::printf("[TEST] FAIL (oot-savestate-roundtrip): Vec3f %s mismatch\n", name);
        return 1;
    }
    return 0;
}

int VerifyBosses() {
    CHECK(sMorphaCore == &sDummyPtrTargets[0], "boss sMorphaCore");
    CHECK(sMorphaTent1 == &sDummyPtrTargets[1], "boss sMorphaTent1");
    CHECK(sMorphaTent2 == &sDummyPtrTargets[2], "boss sMorphaTent2");
    for (int idx : { 0, 299 }) {
        if (CheckVec3fEq(OoT_sEffects[idx].pos, { 42.5f + idx, 85.0f, 127.5f }, "boss sEffects pos") != 0) {
            return 1;
        }
        CHECK(OoT_sEffects[idx].type == static_cast<u8>(3 + idx % 5), "boss sEffects type");
        CHECK(OoT_sEffects[idx].targetPos == reinterpret_cast<Vec3f*>(&sDummyPtrTargets[3]), "boss sEffects targetPos");
        CHECK(OoT_sEffects[idx].epoch == 0xBEEF0000u + idx, "boss sEffects epoch");
    }
    CHECK(sBossMoSeed1 == 1011 && sBossMoSeed2 == 1022 && sBossMoSeed3 == 1033, "boss sBossMoSeed1..3");

    CHECK(sHead == &sDummyPtrTargets[4], "boss sHead");
    CHECK(sHands[0] == &sDummyPtrTargets[5] && sHands[1] == &sDummyPtrTargets[6], "boss sHands");
    CHECK(sFloor == &sDummyPtrTargets[7], "boss sFloor");
    if (CheckVec3fEq(sHandOffsets[0], { 42.5f, 43.5f, 44.5f }, "boss sHandOffsets[0]") != 0 ||
        CheckVec3fEq(sHandOffsets[1], { 45.5f, 46.5f, 47.5f }, "boss sHandOffsets[1]") != 0) {
        return 1;
    }
    CHECK(sHandYawOffsets[0] == 1234 && sHandYawOffsets[1] == -5678, "boss sHandYawOffsets");
    CHECK(sCutsceneCamera == 7 && sBodyStatic == 1u, "boss sCutsceneCamera / sBodyStatic");
    CHECK(sBodyColor.r == 11 && sBodyColor.g == 22 && sBodyColor.b == 33 && sBodyColor.a == 44, "boss sBodyColor");
    CHECK(sStaticColor.r == 55 && sStaticColor.g == 66 && sStaticColor.b == 77 && sStaticColor.a == 88,
          "boss sStaticColor");

    CHECK(sKillBari == 1, "boss sKillBari");
    for (int i = 0; i < 10; i++) {
        CHECK(sBodyBari[i] == static_cast<u8>(0xA0 + i), "boss sBodyBari");
    }
    CHECK(sCsCamera == 12, "boss sCsCamera");
    for (int idx : { 0, 399 }) {
        if (CheckVec3fEq(sVaEffects[idx].pos, { 52.5f, 62.5f, 72.5f }, "boss sVaEffects pos") != 0) {
            return 1;
        }
        CHECK(sVaEffects[idx].type == 6 && sVaEffects[idx].timer == static_cast<u16>(500 + idx), "boss sVaEffects fields");
        CHECK(sVaEffects[idx].parent == reinterpret_cast<BossVa*>(&sDummyPtrTargets[0]), "boss sVaEffects parent");
        CHECK(sVaEffects[idx].epoch == 0xCAFE0000u + idx, "boss sVaEffects epoch");
    }
    CHECK(sBodyState == 4 && sFightPhase == 3 && sCsState == -5, "boss sBodyState / sFightPhase / sCsState");
    CHECK(sDoorState == 9 && sPhase3StopMoving == 1, "boss sDoorState / sPhase3StopMoving");
    CHECK(sZapperRot.x == 100 && sZapperRot.y == -200 && sZapperRot.z == 300, "boss sZapperRot");
    CHECK(sPhase2Timer == 4321 && sPhase4HP == 8, "boss sPhase2Timer / sPhase4HP");
    return 0;
}

int VerifyPlayer() {
    CHECK(D_80858AA0 == 0x10203040 && OoT_sSavedCurrentMask == 7, "player D_80858AA0 / saved mask");
    if (CheckVec3fEq(OoT_sInteractWallCheckResult, { 3.5f, 4.5f, 5.5f }, "player interact wall result") != 0) {
        return 1;
    }
    CHECK(sControlInput == reinterpret_cast<Input*>(&sDummyPtrTargets[1]), "player sControlInput");
    CHECK(OoT_sNoclipEnabled == 1u && OoT_sControlStickMagnitude == 55.25f, "player noclip / stick mag");
    CHECK(OoT_sControlStickAngle == 0x1234 && OoT_sControlStickWorldYaw == -0x2345, "player stick angle / yaw");
    CHECK(OoT_sUpperBodyIsBusy == 1 && sFloorType == 14, "player upper body / floor type");
    CHECK(OoT_sWaterSpeedFactor == 0.5f && OoT_sInvWaterSpeedFactor == 2.0f, "player water speed factor");
    CHECK(sTouchedWallFlags == 0xA5A5A5A5u && sConveyorSpeed == 18u, "player wall flags / conveyor speed");
    CHECK(sIsFloorConveyor == 1 && sConveyorYaw == 0x4000 && sYDistToFloor == -12.75f, "player conveyor / floor dist");
    CHECK(OoT_sPrevFloorProperty == 9 && OoT_sShapeYawToTouchedWall == 0x3000, "player floor prop / shape yaw");
    CHECK(OoT_sWorldYawToTouchedWall == -0x4000 && sFloorShapePitch == 0x0500, "player world yaw / pitch");
    CHECK(sUseHeldItem == 1 && sHeldItemButtonIsHeldDown == 1, "player held item flags");
    return 0;
}

int VerifyAudioOcarina() {
    const u8 b = 0x40;
    const s8 sb = -27;
    const f32 f = 1.375f;
    for (int i = 0; i < 20; i++) {
        const OcarinaNote& m = sOcarinaSongs[OCARINA_SONG_MEMORY_GAME][i];
        const OcarinaNote& sc = sOcarinaSongs[OCARINA_SONG_SCARECROW][i];
        CHECK(m.noteIdx == static_cast<u8>(1 + i) && m.unk_01 == b && m.unk_02 == static_cast<u16>(300 + i) &&
                  m.volume == static_cast<u8>(b + 1) && m.vibrato == static_cast<u8>(b + 2) && m.tone == sb &&
                  m.semitone == 2,
              "audio memory game song");
        CHECK(sc.noteIdx == static_cast<u8>(21 + i) && sc.unk_01 == static_cast<u8>(b + 3) &&
                  sc.unk_02 == static_cast<u16>(600 + i) && sc.volume == static_cast<u8>(b + 4) &&
                  sc.vibrato == static_cast<u8>(b + 5) && sc.tone == static_cast<s8>(sb - 1) && sc.semitone == 3,
              "audio scarecrow song");
    }
    CHECK(sOcaMinigameAppendPos == 11 && sOcaMinigameEndPos == 16, "audio minigame positions");
    CHECK(D_801305B0 == f && D_801305B4 == sb && D_801305B8 == sb + 1 && D_801305BC == sb + 2 && D_801305C0 == sb + 3,
          "audio D_801305B0..C0 fields");
    CHECK(OoT_sAudioIncreasingTranspose == 5 && OoT_sPrevChargeLevel == 2 && D_801305F4 == f + 1.0f,
          "audio transpose / charge / sword freq");
    CHECK(D_80130600 == 18 && D_80130608 == -3 && OoT_sAudioCutsceneFlag == 1, "audio ganons timer / cutscene flag");
    CHECK(OoT_sSpecReverb == 40 && OoT_sAudioEnvReverb == 25 && OoT_sAudioCodeReverb == 15, "audio reverbs");
    CHECK(OoT_sPrevSeqMode == 0x82 && sAudioEnemyDist == f + 100.0f && sAudioEnemyVol == 64, "audio enemy seq");
    CHECK(OoT_sPrevMainBgmSeqId == 0x0045 && D_8013062C == 7 && D_80130630 == 29, "audio prev bgm / resume");
    CHECK(OoT_sNumFramesStill == 12345u && OoT_sNumFramesMoving == 67890u, "audio num frames still / moving");
    CHECK(OoT_sAudioBaseFilter == 0x22 && OoT_sAudioExtraFilter == 0x33 && OoT_sAudioBaseFilter2 == 0x44 &&
              OoT_sAudioExtraFilter2 == 0x55,
          "audio filters");
    CHECK(sSariaBgmPtr == reinterpret_cast<Vec3f*>(&sDummyPtrTargets[2]) && D_80130650 == f + 200.0f, "audio saria bgm");
    CHECK(D_8016B7A8 == f + 2.0f && D_8016B7AC == f + 3.0f && D_8016B7B0 == f + 4.0f && D_8016B7B4 == f + 5.0f,
          "audio D_8016B7A8..B4 fields");
    CHECK(OoT_sRiverFreqScaleLerp.value == f + 0.1f && OoT_sRiverFreqScaleLerp.target == f + 0.2f &&
              OoT_sRiverFreqScaleLerp.step == f + 0.3f && OoT_sRiverFreqScaleLerp.remainingFrames == 40,
          "audio river freq scale lerp");
    CHECK(OoT_sWaterfallFreqScaleLerp.value == f + 0.4f && OoT_sWaterfallFreqScaleLerp.target == f + 0.5f &&
              OoT_sWaterfallFreqScaleLerp.step == f + 0.6f && OoT_sWaterfallFreqScaleLerp.remainingFrames == 20,
          "audio waterfall freq scale lerp");
    CHECK(D_8016B7D8 == f + 6.0f && D_8016B7DC == -12 && D_8016B7E0 == f + 7.0f, "audio D_8016B7D8..E0 fields");
    CHECK(D_8016B8B0 == 90 && D_8016B8B1 == 91 && D_8016B8B2 == 1 && D_8016B8B3 == 1 && sAudioGanonDistVol == 0x77,
          "audio river sound / ganon dist vol");
    for (int i = 0; i < 0x10; i++) {
        const SfxPlayerState& st = OoT_sSfxChannelState[i];
        CHECK(st.vol == f + i && st.freqScale == f + 0.5f + i && st.reverb == static_cast<s8>(sb + i) &&
                  st.panSigned == static_cast<s8>(i - 8) && st.stereoBits == 3 && st.filter == static_cast<u8>(b + i) &&
                  st.unk_0C == 1,
              "audio sfx channel state");
    }
    CHECK(D_8016B9D8 == 9 && D_8016B9F2 == 1 && D_8016B9F3 == 4 && D_8016B9F4 == 5 && D_8016B9F6 == 0x1234,
          "audio malon / fanfare");
    CHECK(OoT_sRecordingState == 2 && OoT_sRecordSongPos == 14 && D_80131860 == 0x89ABCDEFu && D_80131864 == 0x18 &&
              D_80131868 == 0x7F && D_8013186C == 0x0A && D_80131870 == -6 && D_80131874 == 4 && D_80131878 == 13 &&
              D_8013187C == 8 && OoT_sOcarinaDropInputTimer == 3,
          "audio ocarina recording");
    CHECK(OoT_sPlayingStaff.noteIdx == 5 && OoT_sPlayingStaff.state == 2 && OoT_sPlayingStaff.pos == 6,
          "audio playing staff");
    CHECK(sDisplayedStaff.noteIdx == 7 && sDisplayedStaff.state == 3 && sDisplayedStaff.pos == 4, "audio displayed staff");
    CHECK(OoT_sRecordingStaff.noteIdx == 9 && OoT_sRecordingStaff.state == 1 && OoT_sRecordingStaff.pos == 12,
          "audio recording staff");
    CHECK(OoT_sNotePlaybackVolume == 0x60 && OoT_sNotePlaybackVibrato == 0x12 && sNotePlaybackTone == -15 &&
              sNormalizedNotePlaybackTone == f + 8.0f && sNormalizedNotePlaybackVolume == f + 9.0f &&
              D_80130F68 == 987654 && D_80130F4C == 0xC001 && D_8016BA04 == 0xFEDCBA98u && sCurOcaStick.x == 50 &&
              sCurOcaStick.y == -60,
          "audio note playback / stick");
    for (int i = 0; i < 0x10; i++) {
        CHECK(sLearnSongPos[i] == static_cast<u16>(0x100 + i), "audio sLearnSongPos restored");
        CHECK(D_8016BA50[i] == static_cast<u16>(0x200 + i), "audio D_8016BA50 restored");
        CHECK(D_8016BA70[i] == static_cast<u16>(0x300 + i), "audio D_8016BA70 restored");
        CHECK(sLearnSongExpectedNote[i] == static_cast<u8>(0x40 + i), "audio sLearnSongExpectedNote restored");
    }
    return 0;
}

} // namespace

extern "C" int OoT_SaveStateRoundtrip_RunHeadless(void) {
    BaselineGuard guard;
    std::unique_ptr<PlayState> play(new PlayState());
    std::memset(play.get(), 0, sizeof(PlayState));

    TransitionActorEntry entries[3] = {};
    entries[0].id = -12;
    entries[1].id = 45;
    entries[2].id = -300;
    play->transiActorCtx.numActors = ARRAY_COUNT(entries);
    play->transiActorCtx.list = entries;
    OoT_gPlayState = play.get();

    // Seed sentinels for all 5 upstream savestate packets + unserialized sibling song row.
    PopulateEffectContext(OoT_sEffectContext, true);
    SeedBosses(true);
    SeedPlayer(true);
    SeedAudioOcarina(true);
    const u8 savedMinuetNote0 = sOcarinaSongs[0][0].noteIdx;
    sOcarinaSongs[0][0].noteIdx = 0x5A;

    TestSaveState state;
    state.Save();

    // Scribble every serialized field + sibling unserialized row + transition actors.
    PopulateEffectContext(OoT_sEffectContext, false);
    SeedBosses(false);
    SeedPlayer(false);
    SeedAudioOcarina(false);
    sOcarinaSongs[0][0].noteIdx = 0x7E;
    entries[0].id = 999;
    entries[1].id = 888;
    entries[2].id = 777;

    state.Load();

    std::unique_ptr<EffectContext> expectedEffect(new EffectContext());
    PopulateEffectContext(*expectedEffect, true);
    CHECK(std::memcmp(&OoT_sEffectContext, expectedEffect.get(), sizeof(EffectContext)) == 0,
          "OoT_sEffectContext roundtrip");
    if (VerifyBosses() != 0 || VerifyPlayer() != 0 || VerifyAudioOcarina() != 0) {
        sOcarinaSongs[0][0].noteIdx = savedMinuetNote0;
        return 1;
    }
    const u8 siblingAfterLoad = sOcarinaSongs[0][0].noteIdx;
    sOcarinaSongs[0][0].noteIdx = savedMinuetNote0;
    CHECK(siblingAfterLoad == 0x7E, "unserialized sOcarinaSongs row must stay untouched");
    CHECK(entries[0].id == -12 && entries[1].id == 45 && entries[2].id == -300, "transitionActorIds roundtrip");

    // Bounds clamp: live numActors smaller than saved count must not overrun live list.
    play->transiActorCtx.numActors = 2;
    entries[0].id = 111;
    entries[1].id = 222;
    entries[2].id = 333;
    state.Load();
    CHECK(entries[0].id == -12 && entries[1].id == 45 && entries[2].id == 333, "transitionActorIds clamp");

    // Null guards: non-zero numActors with null list, and null OoT_gPlayState. The slot first
    // holds a real three-entry save (count 3, ids -12 / 45 / 333), so the null-list save below
    // must reset the saved count rather than inherit it from the fresh, zeroed SaveStateInfo.
    play->transiActorCtx.numActors = 3;
    TestSaveState nullListState;
    nullListState.Save();
    play->transiActorCtx.numActors = 5;
    play->transiActorCtx.list = nullptr;
    nullListState.Save();
    nullListState.Load();
    OoT_gPlayState = nullptr;
    nullListState.Load();

    play->transiActorCtx.numActors = 3;
    play->transiActorCtx.list = entries;
    OoT_gPlayState = play.get();
    entries[0].id = 10;
    entries[1].id = 20;
    entries[2].id = 30;
    nullListState.Load();
    CHECK(entries[0].id == 10 && entries[1].id == 20 && entries[2].id == 30,
          "null list save resets the saved count to 0");

    std::printf("[TEST] PASS: oot-savestate-roundtrip\n");
    return 0;
}

#undef CHECK

#endif // RSBS_SINGLE_EXECUTABLE
