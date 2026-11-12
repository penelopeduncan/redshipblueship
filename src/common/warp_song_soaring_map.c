/**
 * @file warp_song_soaring_map.c
 * @brief The Termina owl map's decisions and MM's page data (#1169 lane XW8).
 *        The reasoning is in warp_song_soaring_map.h; OoT's draw and pause
 *        states are games/oot/soh/Enhancements/randomizer/SoaringMapOoT.c.
 */

#include "warp_song_soaring_map.h"

#include <string.h>

// The resource paths, as MM's own assets name them (games/mm/assets, the dg*
// defines; soaring-map-tables checks every literal against them). Aligned as
// the games' ALIGN_ASSET(2) arrays are: the renderer tells a path from a
// segmented address by its leading "__OTR__", and the games keep the arrays
// at an even address.
#if defined(_MSC_VER)
#define COMBO_SOARING_MAP_ALIGN __declspec(align(2))
#else
#define COMBO_SOARING_MAP_ALIGN __attribute__((aligned(2)))
#endif

#define COMBO_SOARING_FIELD(name) "__OTR__icon_item_field_static/" name
#define COMBO_SOARING_NAME(name) "__OTR__map_name_static/" name

static const COMBO_SOARING_MAP_ALIGN char kComboSoaringWorldImageTex[] = COMBO_SOARING_FIELD("gWorldMapImageTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringWorldImageTlut[] = COMBO_SOARING_FIELD("gWorldMapImageTLUT");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringOwlFaceTex[] = COMBO_SOARING_FIELD("gWorldMapOwlFaceTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringTitleTex[] = "__OTR__icon_item_jpn_static/gPauseMap10ENGTex";

// MM's sCloudTextures (z_kaleido_map.c) order: bit n of worldMapCloudVisibility.
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud0[] = COMBO_SOARING_FIELD("gWorldMapClockTownCloud1Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud1[] = COMBO_SOARING_FIELD("gWorldMapClockTownCloud2Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud2[] = COMBO_SOARING_FIELD("gWorldMapWoodfallCloud1Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud3[] = COMBO_SOARING_FIELD("gWorldMapWoodfallCloud2Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud4[] = COMBO_SOARING_FIELD("gWorldMapWoodfallCloud3Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud5[] = COMBO_SOARING_FIELD("gWorldMapSnowheadCloud1Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud6[] = COMBO_SOARING_FIELD("gWorldMapSnowheadCloud2Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud7[] = COMBO_SOARING_FIELD("gWorldMapSnowheadCloud3Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud8[] = COMBO_SOARING_FIELD("gWorldMapRomaniRanchCloudTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud9[] = COMBO_SOARING_FIELD("gWorldMapGreatBayCloud1Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud10[] = COMBO_SOARING_FIELD("gWorldMapGreatBayCloud2Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud11[] = COMBO_SOARING_FIELD("gWorldMapGreatBayCloud3Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud12[] = COMBO_SOARING_FIELD("gWorldMapGreatBayCloud4Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud13[] = COMBO_SOARING_FIELD("gWorldMapStoneTowerCloud1Tex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringCloud14[] = COMBO_SOARING_FIELD("gWorldMapStoneTowerCloud2Tex");

// MM's owl names (Kaleido_LoadMapNameStatic's gMapNameStatics), owl order.
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName0[] = COMBO_SOARING_NAME("gMapPointGreatBayCoastENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName1[] = COMBO_SOARING_NAME("gMapPointZoraCapeENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName2[] = COMBO_SOARING_NAME("gMapPointSnowheadENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName3[] = COMBO_SOARING_NAME("gMapPointMountainVillageENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName4[] = COMBO_SOARING_NAME("gMapPointClockTownENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName5[] = COMBO_SOARING_NAME("gMapPointMilkRoadENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName6[] = COMBO_SOARING_NAME("gMapPointWoodfallENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName7[] = COMBO_SOARING_NAME("gMapPointSouthernSwampENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName8[] = COMBO_SOARING_NAME("gMapPointIkanaCanyonENGTex");
static const COMBO_SOARING_MAP_ALIGN char kComboSoaringName9[] = COMBO_SOARING_NAME("gMapPointStoneTowerENGTex");

static const char* const kComboSoaringCloudTexs[RSBS_SOARING_CLOUD_COUNT] = {
    kComboSoaringCloud0,  kComboSoaringCloud1,  kComboSoaringCloud2,  kComboSoaringCloud3,  kComboSoaringCloud4,
    kComboSoaringCloud5,  kComboSoaringCloud6,  kComboSoaringCloud7,  kComboSoaringCloud8,  kComboSoaringCloud9,
    kComboSoaringCloud10, kComboSoaringCloud11, kComboSoaringCloud12, kComboSoaringCloud13, kComboSoaringCloud14,
};

static const char* const kComboSoaringNameTexs[RSBS_SOARING_OWL_COUNT] = {
    kComboSoaringName0, kComboSoaringName1, kComboSoaringName2, kComboSoaringName3, kComboSoaringName4,
    kComboSoaringName5, kComboSoaringName6, kComboSoaringName7, kComboSoaringName8, kComboSoaringName9,
};

static const char* const kComboSoaringAssets[] = {
    kComboSoaringWorldImageTex, kComboSoaringWorldImageTlut, kComboSoaringOwlFaceTex, kComboSoaringTitleTex,
    kComboSoaringCloud0,        kComboSoaringCloud1,         kComboSoaringCloud2,     kComboSoaringCloud3,
    kComboSoaringCloud4,        kComboSoaringCloud5,         kComboSoaringCloud6,     kComboSoaringCloud7,
    kComboSoaringCloud8,        kComboSoaringCloud9,         kComboSoaringCloud10,    kComboSoaringCloud11,
    kComboSoaringCloud12,       kComboSoaringCloud13,        kComboSoaringCloud14,    kComboSoaringName0,
    kComboSoaringName1,         kComboSoaringName2,          kComboSoaringName3,      kComboSoaringName4,
    kComboSoaringName5,         kComboSoaringName6,          kComboSoaringName7,      kComboSoaringName8,
    kComboSoaringName9,
};

// MM's sVtxPageMapWorldQuadsX/Y and gVtxPageMapWorldQuadsWidth/Height
// (z_kaleido_scope_NES.c): rows 0..14 are the clouds, rows 26..35 the owls.
static const ComboSoaringQuad kComboSoaringCloudQuads[RSBS_SOARING_CLOUD_COUNT] = {
    { -41, 11, 80, 40 }, { 9, 22, 64, 31 },    { 43, -16, 64, 53 },  { -21, -16, 64, 53 }, { -37, -16, 16, 53 },
    { 43, 59, 64, 52 },  { -69, 24, 112, 35 }, { -69, 59, 112, 35 }, { -32, -11, 48, 32 }, { -109, -5, 64, 64 },
    { -45, -5, 64, 64 }, { -109, 59, 64, 64 }, { -45, 59, 64, 64 },  { 59, 39, 48, 82 },   { -5, 31, 64, 61 },
};

static const ComboSoaringQuad kComboSoaringOwlQuads[RSBS_SOARING_OWL_COUNT] = {
    { -80, -8, 24, 12 }, { -64, -38, 24, 12 }, { -9, 39, 24, 12 },  { -3, 26, 24, 12 }, { -7, 1, 24, 12 },
    { -16, -7, 24, 12 }, { -1, -28, 24, 12 },  { 23, -27, 24, 12 }, { 44, -1, 24, 12 }, { 54, 24, 24, 12 },
};

// MM's sOwlWarpWorldMapCursorsX/Y.
static const float kComboSoaringCursorX[RSBS_SOARING_OWL_COUNT] = { -50.0f, -38.0f, 6.0f,  11.0f, 8.0f,
                                                                    0.0f,   12.0f,  31.0f, 48.0f, 56.0f };
static const float kComboSoaringCursorY[RSBS_SOARING_OWL_COUNT] = { -14.0f, -39.0f, 23.0f,  11.0f,  -8.0f,
                                                                    -15.0f, -31.0f, -30.0f, -10.0f, 11.0f };

// sOwlWarpPauseItems[owl] - ITEM_MAP_POINT_GREAT_BAY: the map_name_static index.
static const int8_t kComboSoaringNameIndex[RSBS_SOARING_OWL_COUNT] = { 11, 15, 6, 13, 5, 14, 4, 12, 8, 10 };

bool Combo_SoaringMap_IsMapState(uint16_t state) {
    switch (state) {
        case RSBS_OOT_PAUSE_SOARING_0:
        case RSBS_OOT_PAUSE_SOARING_1:
        case RSBS_OOT_PAUSE_SOARING_2:
        case RSBS_OOT_PAUSE_SOARING_3:
        case RSBS_OOT_PAUSE_SOARING_SELECT:
        case RSBS_OOT_PAUSE_SOARING_CONFIRM:
        case RSBS_OOT_PAUSE_SOARING_6:
            return true;
        default:
            return false;
    }
}

bool Combo_SoaringMap_OwnsUpdate(uint16_t state, bool closing) {
    return Combo_SoaringMap_IsMapState(state) || (closing && state == RSBS_OOT_PAUSE_UNPAUSE_SETUP);
}

bool Combo_SoaringMap_DrawsState(uint16_t state) {
    return Combo_SoaringMap_IsMapState(state) && state != RSBS_OOT_PAUSE_SOARING_0 && state != RSBS_OOT_PAUSE_SOARING_1;
}

bool Combo_SoaringMap_Open(ComboSoaringMap* map, uint16_t mask) {
    if (map == NULL) {
        return false;
    }
    memset(map, 0, sizeof(*map));
    map->mask = (uint16_t)(mask & RSBS_SOARING_OWL_BITS);
    map->cursor = (int8_t)Combo_SoaringFirstOwl(map->mask);
    map->result = -1;
    if (map->cursor < 0) {
        map->phase = COMBO_SOARING_MAP_DONE;
        return false;
    }
    map->phase = COMBO_SOARING_MAP_SELECT;
    return true;
}

int Combo_SoaringPrevOwl(uint16_t owlMask, int current) {
    owlMask &= RSBS_SOARING_OWL_BITS;
    if (owlMask == 0) {
        return -1;
    }
    if (current < 0 || current >= RSBS_SOARING_OWL_COUNT) {
        current = 0;
    }
    for (int step = 1; step <= RSBS_SOARING_OWL_COUNT; step++) {
        const int owl = (current - step + RSBS_SOARING_OWL_COUNT) % RSBS_SOARING_OWL_COUNT;
        if ((owlMask >> owl) & 1u) {
            return owl;
        }
    }
    return -1;
}

int Combo_SoaringMap_Stick(ComboSoaringMap* map, int16_t stickX, bool dLeftPress, bool dRightPress, bool dpadOn) {
    if (map == NULL) {
        return 0;
    }
    const int dir = stickX < -RSBS_SOARING_MAP_STICK_THRESHOLD ? -1 : stickX > RSBS_SOARING_MAP_STICK_THRESHOLD ? 1 : 0;
    int out = 0;
    if (dir == 0) {
        map->repDir = 0;
    } else if (map->repDir == dir) {
        // MM: decrement while held; register when it drops below 0, then 2.
        map->repTimer--;
        if (map->repTimer < 0) {
            map->repTimer = 2;
            out = dir;
        }
    } else {
        // MM: the first input registers at once and arms the long delay.
        map->repTimer = 10;
        map->repDir = (int8_t)dir;
        out = dir;
    }
    if (out == 0 && dpadOn) {
        if (dRightPress && !dLeftPress) {
            out = 1;
        } else if (dLeftPress && !dRightPress) {
            out = -1;
        }
    }
    return out;
}

ComboSoaringMapEvent Combo_SoaringMap_Step(ComboSoaringMap* map, const ComboSoaringMapInput* input) {
    if (map == NULL || input == NULL) {
        return COMBO_SOARING_MAP_EV_NONE;
    }
    switch (map->phase) {
        case COMBO_SOARING_MAP_SELECT:
            if (input->b || input->start) {
                map->result = -1;
                map->phase = COMBO_SOARING_MAP_DONE;
                return COMBO_SOARING_MAP_EV_CANCEL;
            }
            if (input->a) {
                map->phase = COMBO_SOARING_MAP_CONFIRM;
                return COMBO_SOARING_MAP_EV_OPEN_CONFIRM;
            }
            if (input->dir != 0) {
                const int next = input->dir > 0 ? Combo_SoaringNextOwl(map->mask, map->cursor)
                                                : Combo_SoaringPrevOwl(map->mask, map->cursor);
                if (next >= 0 && next != map->cursor) {
                    map->cursor = (int8_t)next;
                    return COMBO_SOARING_MAP_EV_MOVED;
                }
            }
            return COMBO_SOARING_MAP_EV_NONE;
        case COMBO_SOARING_MAP_CONFIRM:
            if (input->a) {
                if (input->choice == 0) {
                    map->result = map->cursor;
                    map->phase = COMBO_SOARING_MAP_DONE;
                    return COMBO_SOARING_MAP_EV_DECIDE;
                }
                map->phase = COMBO_SOARING_MAP_SELECT;
                return COMBO_SOARING_MAP_EV_BACK;
            }
            if (input->b) {
                map->phase = COMBO_SOARING_MAP_SELECT;
                return COMBO_SOARING_MAP_EV_BACK;
            }
            if (input->start) {
                map->result = -1;
                map->phase = COMBO_SOARING_MAP_DONE;
                return COMBO_SOARING_MAP_EV_CANCEL;
            }
            if (input->advanceOther || !input->boxOpen) {
                map->phase = COMBO_SOARING_MAP_SELECT;
                return COMBO_SOARING_MAP_EV_BACK;
            }
            return COMBO_SOARING_MAP_EV_NONE;
        default:
            return COMBO_SOARING_MAP_EV_NONE;
    }
}

bool Combo_SoaringMap_EventCloses(ComboSoaringMapEvent ev) {
    return ev == COMBO_SOARING_MAP_EV_CANCEL || ev == COMBO_SOARING_MAP_EV_DECIDE;
}

uint16_t Combo_SoaringMap_HudRestoreOnEvent(ComboSoaringMapEvent ev) {
    return Combo_SoaringMap_EventCloses(ev) ? (uint16_t)RSBS_OOT_HUD_RESTORE_ALL : (uint16_t)0;
}

uint8_t Combo_SoaringMap_OcarinaModeOnHandBack(void) {
    return (uint8_t)RSBS_OOT_OCARINA_MODE_END;
}

ComboSoaringRoute Combo_SoaringMap_Route(ComboSoaringGate gate, bool assetsPresent, bool pauseCanOpen) {
    if (gate != COMBO_SOARING_GATE_OFFER) {
        return COMBO_SOARING_ROUTE_REFUSE;
    }
    return (assetsPresent && pauseCanOpen) ? COMBO_SOARING_ROUTE_MAP : COMBO_SOARING_ROUTE_CHOOSER;
}

bool Combo_SoaringMap_PauseCanOpen(const ComboSoaringPauseFacts* f) {
    if (f == NULL) {
        return false;
    }
    return f->pauseState == 0 && f->debugState == 0 && f->gameOverState == 0 && f->transitionTrigger == 0 &&
           f->transitionMode == 0 && f->csState == 0 && f->cutsceneIndex < 0xFFF0 && f->nextCutsceneIndex < 0xFFF0 &&
           f->shootingGalleryStatus <= 1 && f->magicState != f->magicStepCapacity && f->magicState != f->magicFill &&
           f->gameMode == f->gameModeNormal && f->playerCsAction == 0 && f->playerDead == 0;
}

bool Combo_SoaringMap_OwlQuad(int owl, ComboSoaringQuad* out) {
    if (owl < 0 || owl >= RSBS_SOARING_OWL_COUNT || out == NULL) {
        return false;
    }
    *out = kComboSoaringOwlQuads[owl];
    return true;
}

bool Combo_SoaringMap_OwlCursor(int owl, float* x, float* y) {
    if (owl < 0 || owl >= RSBS_SOARING_OWL_COUNT || x == NULL || y == NULL) {
        return false;
    }
    *x = kComboSoaringCursorX[owl];
    *y = kComboSoaringCursorY[owl];
    return true;
}

int Combo_SoaringMap_OwlNameIndex(int owl) {
    if (owl < 0 || owl >= RSBS_SOARING_OWL_COUNT) {
        return -1;
    }
    return kComboSoaringNameIndex[owl];
}

const char* Combo_SoaringMap_OwlNameTex(int owl) {
    if (owl < 0 || owl >= RSBS_SOARING_OWL_COUNT) {
        return NULL;
    }
    return kComboSoaringNameTexs[owl];
}

bool Combo_SoaringMap_CloudQuad(int n, ComboSoaringQuad* out) {
    if (n < 0 || n >= RSBS_SOARING_CLOUD_COUNT || out == NULL) {
        return false;
    }
    *out = kComboSoaringCloudQuads[n];
    return true;
}

const char* Combo_SoaringMap_CloudTex(int n) {
    if (n < 0 || n >= RSBS_SOARING_CLOUD_COUNT) {
        return NULL;
    }
    return kComboSoaringCloudTexs[n];
}

const char* Combo_SoaringMap_WorldImageTex(void) {
    return kComboSoaringWorldImageTex;
}

const char* Combo_SoaringMap_WorldImageTlut(void) {
    return kComboSoaringWorldImageTlut;
}

const char* Combo_SoaringMap_OwlFaceTex(void) {
    return kComboSoaringOwlFaceTex;
}

const char* Combo_SoaringMap_TitleTex(void) {
    return kComboSoaringTitleTex;
}

const char* const* Combo_SoaringMap_AssetPaths(size_t* count) {
    if (count != NULL) {
        *count = sizeof(kComboSoaringAssets) / sizeof(kComboSoaringAssets[0]);
    }
    return kComboSoaringAssets;
}
