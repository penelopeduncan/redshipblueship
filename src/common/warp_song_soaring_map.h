#pragma once
/**
 * @file warp_song_soaring_map.h
 * @brief The Termina owl map Ocarina of Time's Song of Soaring opens (#1169 lane
 *        XW8; OoTMM packages/generator/src/oot/ovl_patch/kaleido_scope.c and
 *        oot/ocarina.c SetupSoaring). Game-header-free: every decision the map
 *        makes lives here, and games/oot/soh/Enhancements/randomizer/
 *        SoaringMapOoT.c only acts on them.
 *
 * WHAT THE PLAYER SEES. After XW6's gates offer (warp_song_soaring.h), OoT's
 * pause opens in a custom mode of its own kaleido, as OoTMM does: Majora's
 * Mask's owl-warp page (MM's world map image, its clouds where MM's frozen half
 * has not cleared them, the darkening rectangle and the owl face on every
 * ACTIVE statue), MM's own name texture for the owl under the cursor and the
 * blue cursor. The stick (and the D-pad under SoH's DPadOnPause) steps through
 * the active owls in MM's index order, wrapping; A asks "Soar to <owl>?" with
 * OK / No in a 0x88D-style box; OK crosses exactly as XW6 crosses
 * (OoT_SoaringOoT_Depart); No, B, and a box the message system would close
 * itself go back to the map; B or START on the map, and START on the prompt,
 * close it and hand control back.
 *
 * THE PAUSE STATES are OoTMM's (pause_state_defs.h), all above every vanilla
 * state (0..0x13), so no vanilla arm takes them: kaleido's guarded arms
 * (z_kaleido_scope_call.c K1/K2, z_kaleido_scope_PAL.c K3/K4) hand them to the
 * map. The close hands back through vanilla 0x12 (whose one frame the map also
 * owns, see Combo_SoaringMap_OwnsUpdate) and vanilla 0x13, so SoH's
 * PauseBufferInputs sees an ordinary unpause.
 *
 * THE FALLBACK. Combo_SoaringMap_Route sends the song to XW6's text chooser
 * whenever the map cannot open: an asset the map draws is not mounted (a
 * refused or missing curated generation) or a pause precondition fails. The
 * chooser stays until the map is proven in play.
 *
 * INERT UNTIL XW12, like the rest of the song (warp_song_soaring.h).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "warp_song_soaring.h"

#ifdef __cplusplus
extern "C" {
#endif

/** OoTMM's owl-warp pause states (pause_state_defs.h), in OoT's u16 state. */
#define RSBS_OOT_PAUSE_SOARING_0 0x101u       // the letterbox, then the prerender is armed (vanilla 1)
#define RSBS_OOT_PAUSE_SOARING_1 0x102u       // waits for the prerender (vanilla 2)
#define RSBS_OOT_PAUSE_SOARING_2 0x103u       // the button save, the labels, the roll's start
#define RSBS_OOT_PAUSE_SOARING_3 0x104u       // the roll-in
#define RSBS_OOT_PAUSE_SOARING_SELECT 0x106u  // the cursor
#define RSBS_OOT_PAUSE_SOARING_CONFIRM 0x107u // "Soar to <owl>?"
#define RSBS_OOT_PAUSE_SOARING_6 0x112u       // the roll-out
/** Vanilla OoT's unpause states the close hands to. */
#define RSBS_OOT_PAUSE_UNPAUSE_SETUP 0x12u
#define RSBS_OOT_PAUSE_UNPAUSE_CLOSE 0x13u

/** The confirm prompt's text, right after XW6's chooser (SongOfSoaringOoT.cpp
 *  static-asserts the order). Built by an OnOpenText hook. */
#define RSBS_OOT_TEXT_SOARING_CONFIRM 0x9216u

/** The HUD mode vanilla TEXT_CLOSING restores after a box (z_message_PAL.c:
 *  unk_13EE 0, 1 or 2 becomes 0x32), which every close of the map writes. */
#define RSBS_OOT_HUD_RESTORE_ALL 0x32u

/** The longest any timed state of the map may last before it is forced closed. */
#define RSBS_SOARING_MAP_FRAME_CAP 30

/** MM's world-map clouds (WORLD_MAP_NUM_CLOUDS) and the stick threshold. */
#define RSBS_SOARING_CLOUD_COUNT 15
#define RSBS_SOARING_MAP_STICK_THRESHOLD 30

/** Whether @p state is one of the seven map states. */
bool Combo_SoaringMap_IsMapState(uint16_t state);
/** Whether OoT's kaleido update hands @p state to the map: a map state, or the
 *  one vanilla 0x12 frame of a close the map started (@p closing). */
bool Combo_SoaringMap_OwnsUpdate(uint16_t state, bool closing);
/** Whether the call layer draws @p state for the map (K2): the five states from
 *  SOARING_2 on (SOARING_0 and _1 wait for the prerender, as vanilla 1 and 2). */
bool Combo_SoaringMap_DrawsState(uint16_t state);

// ---------------------------------------------------------------------------
// The picker
// ---------------------------------------------------------------------------

typedef enum {
    COMBO_SOARING_MAP_SELECT = 0,
    COMBO_SOARING_MAP_CONFIRM = 1,
    COMBO_SOARING_MAP_DONE = 2,
} ComboSoaringMapPhase;

typedef struct ComboSoaringMap {
    uint16_t mask;   // the active owls (bits 0..9 only)
    int8_t cursor;   // the owl under the cursor, -1 when closed
    uint8_t phase;   // ComboSoaringMapPhase
    int8_t result;   // DONE: the chosen owl, or -1 for a cancel
    int8_t repDir;   // MM's stick repeat: the held direction (-1, 0, 1)
    int8_t repTimer; // MM's stick repeat: frames until the next repeat
} ComboSoaringMap;

/** One frame's input, filled by OoT code. */
typedef struct ComboSoaringMapInput {
    int8_t dir;        // Combo_SoaringMap_Stick's answer this frame
    bool a;            // A pressed
    bool b;            // B pressed
    bool start;        // START pressed
    bool advanceOther; // OoT_Message_ShouldAdvanceSilent without A: C-up, or B held under SkipText
    bool boxOpen;      // the confirm box is still open
    uint8_t choice;    // msgCtx.choiceIndex (0 OK, 1 No)
} ComboSoaringMapInput;

typedef enum {
    COMBO_SOARING_MAP_EV_NONE = 0,
    COMBO_SOARING_MAP_EV_MOVED = 1,        // NA_SE_SY_CURSOR
    COMBO_SOARING_MAP_EV_OPEN_CONFIRM = 2, // NA_SE_SY_DECIDE, then "Soar to <owl>?"
    COMBO_SOARING_MAP_EV_BACK = 3,         // the prompt goes, back to the map (NA_SE_SY_MESSAGE_PASS)
    COMBO_SOARING_MAP_EV_CANCEL = 4,       // the map closes, nothing chosen
    COMBO_SOARING_MAP_EV_DECIDE = 5,       // the map closes on the owl under the cursor
} ComboSoaringMapEvent;

/** Opens the picker over @p mask (bits 0..9; MM's hidden owl, bit 15, never
 *  counts): the cursor on Clock Town when active, else the lowest owl (MM's
 *  func_800F4A10). False, with the picker closed, when no owl is active. */
bool Combo_SoaringMap_Open(ComboSoaringMap* map, uint16_t mask);

/** The previous active owl before @p current, wrapping (MM's do-while,
 *  z_kaleido_map.c); -1 for an empty mask. Combo_SoaringNextOwl is the other
 *  direction. */
int Combo_SoaringPrevOwl(uint16_t owlMask, int current);

/**
 * The direction the cursor moves this frame: MM's stick repeat exactly
 * (z_kaleido_scope_NES.c), a held stick past ±30 registering on its first frame,
 * again 11 frames later and then every third frame (0, 11, 14, 17, 20...);
 * otherwise, when @p dpadOn (SoH's DPadOnPause), a D-pad press registers on its
 * press frame. Called every frame the picker is open.
 */
int Combo_SoaringMap_Stick(ComboSoaringMap* map, int16_t stickX, bool dLeftPress, bool dRightPress, bool dpadOn);

/**
 * One frame of the picker. Buttons first, then movement only while still on the
 * map (MM's order: the A frame never moves the cursor).
 *   SELECT:  B or START cancels; A opens the prompt; a direction moves.
 *   CONFIRM: A on OK decides; A on No, B, advanceOther (a stated divergence:
 *            OoT's message system would close the box itself) and a box that
 *            vanished go back; START cancels.
 *   DONE:    nothing.
 */
ComboSoaringMapEvent Combo_SoaringMap_Step(ComboSoaringMap* map, const ComboSoaringMapInput* input);

/** True for the events that close the map (CANCEL, DECIDE). */
bool Combo_SoaringMap_EventCloses(ComboSoaringMapEvent ev);
/** The HUD restore value an event writes into gSaveContext.unk_13EE before the
 *  close (RSBS_OOT_HUD_RESTORE_ALL for CANCEL and DECIDE), 0 for the others. */
uint16_t Combo_SoaringMap_HudRestoreOnEvent(ComboSoaringMapEvent ev);
/** The ocarina mode every hand-back leaves: OCARINA_MODE_04. */
uint8_t Combo_SoaringMap_OcarinaModeOnHandBack(void);

// ---------------------------------------------------------------------------
// The route: map, chooser or refusal
// ---------------------------------------------------------------------------

typedef enum {
    COMBO_SOARING_ROUTE_REFUSE = 0,  // XW6's refusal text, unchanged
    COMBO_SOARING_ROUTE_MAP = 1,     // the owl map
    COMBO_SOARING_ROUTE_CHOOSER = 2, // XW6's interim text chooser, the fallback
} ComboSoaringRoute;

ComboSoaringRoute Combo_SoaringMap_Route(ComboSoaringGate gate, bool assetsPresent, bool pauseCanOpen);

/** The facts a pause opening depends on, as plain integers OoT code fills. The
 *  three constants are OoT's MAGIC_STATE_STEP_CAPACITY, MAGIC_STATE_FILL and
 *  GAMEMODE_NORMAL, passed in so this file needs no OoT header. */
typedef struct ComboSoaringPauseFacts {
    int32_t pauseState;
    int32_t debugState;
    int32_t gameOverState; // 0: GAMEOVER_INACTIVE
    int32_t transitionTrigger;
    int32_t transitionMode;
    int32_t csState; // play->csCtx.state, 0: CS_STATE_IDLE
    int32_t cutsceneIndex;
    int32_t nextCutsceneIndex;
    int32_t shootingGalleryStatus;
    int32_t magicState;
    int32_t gameMode;
    int32_t playerCsAction; // player->csAction
    int32_t playerDead;     // Player_InCsMode is NOT a fact: the ocarina sets PLAYER_STATE1_IN_CUTSCENE
    int32_t magicStepCapacity;
    int32_t magicFill;
    int32_t gameModeNormal;
} ComboSoaringPauseFacts;

/**
 * KaleidoSetup_Update's conditions for a pause (z_kaleido_setup.c) one by one,
 * WITHOUT OoT_Play_InCsMode: taking out the ocarina sets PLAYER_STATE1_IN_CUTSCENE
 * (z_player.c), which that predicate reads, so it is always true here. The
 * Bombchu Bowling switch is left out too (it guards START only). Plus a normal
 * game mode, no player cutscene action and a living Link.
 */
bool Combo_SoaringMap_PauseCanOpen(const ComboSoaringPauseFacts* facts);

// ---------------------------------------------------------------------------
// The page's data: MM's own tables (z_kaleido_scope_NES.c, z_kaleido_map.c)
// ---------------------------------------------------------------------------

typedef struct ComboSoaringQuad {
    int16_t x; // left edge, map-page units
    int16_t y; // top edge
    int16_t w;
    int16_t h;
} ComboSoaringQuad;

/** The owl face quad (MM's sVtxPageMapWorldQuads owl-warp rows). */
bool Combo_SoaringMap_OwlQuad(int owl, ComboSoaringQuad* out);
/** The cursor's centre in the info-panel view (MM's sOwlWarpWorldMapCursorsX/Y). */
bool Combo_SoaringMap_OwlCursor(int owl, float* x, float* y);
/** MM's map_name_static index of the owl's name (sOwlWarpPauseItems[owl] -
 *  ITEM_MAP_POINT_GREAT_BAY), -1 out of range. */
int Combo_SoaringMap_OwlNameIndex(int owl);
/** The owl's English name texture (MM's map_name_static), NULL out of range. */
const char* Combo_SoaringMap_OwlNameTex(int owl);
/** Cloud @p n's quad and texture (MM's sCloudTextures order), NULL / false out of range. */
bool Combo_SoaringMap_CloudQuad(int n, ComboSoaringQuad* out);
const char* Combo_SoaringMap_CloudTex(int n);
/** The world map image (CI8 216x128), its TLUT (256 RGBA16), the owl face (RGBA32
 *  24x12) and MM's map-page title quad (IA8 80x32, column 1 row 0). */
const char* Combo_SoaringMap_WorldImageTex(void);
const char* Combo_SoaringMap_WorldImageTlut(void);
const char* Combo_SoaringMap_OwlFaceTex(void);
const char* Combo_SoaringMap_TitleTex(void);
/** Every MM resource the map draws, each with the "__OTR__" prefix; @p count
 *  receives the number. All must be mounted for the map to open. */
const char* const* Combo_SoaringMap_AssetPaths(size_t* count);

/**
 * The clouds MM's frozen half has cleared (bit n set: cloud n is gone, MM's
 * worldMapCloudVisibility), under the same guard as the owl mask (registered
 * layout in range, the marker, day 1 or later); 0 (every cloud drawn) otherwise.
 */
uint16_t Combo_SoaringCloudMaskFromBlob(const uint8_t* blob, size_t size);
/** Combo_SoaringCloudMaskFromBlob over Majora's Mask's frozen half. */
uint16_t Combo_SoaringCloudMask(void);

#ifdef __cplusplus
}
#endif
