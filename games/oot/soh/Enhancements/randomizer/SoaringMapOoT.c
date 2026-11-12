/**
 * @file SoaringMapOoT.c
 * @brief The Termina owl map in Ocarina of Time's pause (#1169 lane XW8;
 *        OoTMM packages/generator/src/oot/ovl_patch/kaleido_scope.c,
 *        KaleidoScope_BeforeUpdate and KaleidoScope_BeforeDraw).
 *
 * A guarded custom mode of OoT's own kaleido, as OoTMM builds it: the song's
 * gates offer (SongOfSoaringOoT.cpp), OoT_SoaringMap_Begin hides the "You
 * played" box and sets the pause state to SOARING_0, and from there OoT's pause
 * machinery runs the map: the call layer arms the prerender (K1), kaleido's
 * update and draw hand their states here (K3, K4), and the close goes through
 * vanilla 0x12 and 0x13, which put the buttons, the HUD, the objects and the
 * update rate back. Every decision (which owls, the cursor, the confirm and
 * cancel outcomes, the route, the pause precondition) is
 * src/common/warp_song_soaring_map.c.
 *
 * THE DRAW is MM's owl-warp page (z_kaleido_scope_NES.c, z_kaleido_map.c) as
 * OoTMM ports it into OoT's renderer: OoT's map-page frame with MM's map title
 * quad, MM's world map image in fifteen angled fragments, MM's clouds where the
 * frozen half has not cleared them, the darkening rectangle, the owl face on
 * every active statue, OoT's name panel with MM's name texture and OoT's four
 * cursor corners at MM's cursor positions. MM's textures are drawn by resource
 * path from the curated half (assets/crossgame/manifest.txt), probed before
 * the map opens. Every vertex is this file's own (Graph_Alloc each frame):
 * kaleido's InitVertices never runs for these states, so pauseCtx's vertex
 * pointers are never read.
 *
 * THE HUD. E1 already fades it to "nothing" (Interface_ChangeAlpha with 1); the map
 * changes the HUD mode only in its backstop. Opening the prompt re-snapshots unk_13EE
 * (Message_OpenText), so every close writes RSBS_OOT_HUD_RESTORE_ALL into it
 * again before vanilla 0x13 applies it (MM: prevHudVisibility = ALL).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "soh/frame_interpolation.h"
#include "soh/Enhancements/randomizer/SoaringMapOoT.h"
#include "soh/Enhancements/randomizer/SongOfSoaringOoT.h"

#include "warp_song_soaring_map.h" // src/common: the picker, the route, MM's page data (#1169 XW8)

#include <stdio.h>
#include <string.h>

// z_kaleido_scope_PAL.c (non-static there, in no header).
void OoT_KaleidoScope_SetView(PauseContext* pauseCtx, f32 x, f32 y, f32 z);
Gfx* OoT_KaleidoScope_DrawPageSections(Gfx* gfx, Vtx* vertices, void** textures);

// MM's page geometry (z_kaleido_scope.h): fifteen 80x32 frame quads, the
// 216x128 CI8 world map in 9-row fragments (TMEM / 2 / 216) and the eye MM's
// func_800F4A10 sets for the map (PAUSE_EYE_DIST * -PAUSE_MAP_X).
#define SOARING_MAP_BG_QUADS 15
#define SOARING_MAP_IMAGE_W 216
#define SOARING_MAP_IMAGE_H 128
#define SOARING_MAP_FRAG_H 9
#define SOARING_MAP_FRAG_NUM (((SOARING_MAP_IMAGE_H - 1) / SOARING_MAP_FRAG_H) + 1)
#define SOARING_MAP_PAGE_QUADS \
    (SOARING_MAP_BG_QUADS + RSBS_SOARING_CLOUD_COUNT + RSBS_SOARING_OWL_COUNT + SOARING_MAP_FRAG_NUM)
#define SOARING_MAP_QUAD_CLOUD0 SOARING_MAP_BG_QUADS
#define SOARING_MAP_QUAD_OWL0 (SOARING_MAP_QUAD_CLOUD0 + RSBS_SOARING_CLOUD_COUNT)
#define SOARING_MAP_QUAD_IMAGE0 (SOARING_MAP_QUAD_OWL0 + RSBS_SOARING_OWL_COUNT)
#define SOARING_MAP_PANEL_VTX 20
#define SOARING_MAP_ROLL_CLOSED 160.0f
#define SOARING_MAP_PAGE_HIDDEN_Y (-6240.0f)

typedef struct SoaringMapOoTState {
    ComboSoaringMap picker;
    bool open;              // Begin ran, the close's end has not
    bool closing;           // the close handed the pause to vanilla 0x12 / 0x13
    bool buttonSavePending; // the first kaleido update copies buttonStatus
    bool buttonSaveTaken;   // buttonSave holds that copy (the backstop puts it back)
    bool soundDucked;       // Begin's func_800F64E0(1) has not met the close's (0)
    s16 frames;             // frames in the current timed state
    f32 roll;               // MM's mapPageRoll: 160 rolled away, 0 facing
    f32 pageOffsetY;        // MM's sPauseMenuVerticalOffset (WREG(2) in OoT)
    s16 alpha;              // MM's pauseCtx->alpha for the page
    s16 owlAlpha;           // MM's R_PAUSE_OWL_WARP_ALPHA (the darkening)
    s16 infoOffsetY;        // MM's infoPanelOffsetY
    f32 cursorShrink;       // MM's cursorShrinkRate
    s16 cursorPrim[3];
    s16 cursorEnv[3];
    s16 cursorColorTimer;
    s16 cursorColorIndex;
    uint16_t clouds;                                       // the frozen half's cleared clouds, read at Begin
    u8 buttonSave[ARRAY_COUNT(gSaveContext.buttonStatus)]; // K3's button save, for the backstop
} SoaringMapOoTState;

static SoaringMapOoTState sSoaringMapOoT;

// MM's cursor colour set BLUE (OoTMM sCursorPrim/EnvColorTarget rows 4 and 5).
static const s16 kSoaringMapCursorPrim[2][3] = { { 100, 150, 255 }, { 100, 255, 255 } };
static const s16 kSoaringMapCursorEnv[2][3] = { { 0, 0, 100 }, { 0, 150, 255 } };

static void SoaringMapPlaySfx(u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, &OoT_gSfxDefaultPos, 4, &OoT_gSfxDefaultFreqAndVolScale,
                           &OoT_gSfxDefaultFreqAndVolScale, &OoT_gSfxDefaultReverb);
}

/** The confirm (or "You played") box, gone at once, as MM's and OoTMM's owl
 *  states close it (msgLength 0, MSGMODE_NONE), without vanilla's TEXT_CLOSING
 *  (which would call Interface_ChangeAlpha in the middle of the pause). */
static void SoaringMapHardClearBox(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;
    msgCtx->msgLength = 0;
    msgCtx->msgMode = MSGMODE_NONE;
    msgCtx->stateTimer = 0;
    msgCtx->textboxEndType = TEXTBOX_ENDTYPE_DEFAULT;
}

static bool SoaringMapConfirmBoxOpen(PlayState* play) {
    const MessageContext* msgCtx = &play->msgCtx;
    return msgCtx->msgLength != 0 && msgCtx->msgMode != MSGMODE_NONE && msgCtx->textId == RSBS_OOT_TEXT_SOARING_CONFIRM;
}

static void SoaringMapSetLabels(PlayState* play) {
    // OoTMM OWLWARP_2 and every BACK (kaleido_scope.c): A decides, B stops.
    Interface_SetDoAction(play, DO_ACTION_DECIDE);
    Interface_LoadActionLabelB(play, DO_ACTION_STOP);
}

/** CANCEL or DECIDE (and a timed state's cap): the map starts its roll-out. */
static void SoaringMapEnterClose(PlayState* play, ComboSoaringMapEvent ev) {
    if (SoaringMapConfirmBoxOpen(play)) {
        SoaringMapHardClearBox(play);
    }
    Interface_SetDoAction(play, DO_ACTION_NONE);
    func_800F64E0(0); // the close sound at the transition, as MM, OoTMM and vanilla play it
    sSoaringMapOoT.soundDucked = false;
    // After the prompt's re-snapshot: the HUD comes back when vanilla 0x13
    // applies unk_13EE (MM sets prevHudVisibility = HUD_VISIBILITY_ALL).
    gSaveContext.unk_13EE = Combo_SoaringMap_HudRestoreOnEvent(ev); // RSBS_OOT_HUD_RESTORE_ALL
    if (ev == COMBO_SOARING_MAP_EV_DECIDE) {
        SoaringMapPlaySfx(NA_SE_SY_DECIDE);
    }
    if (sSoaringMapOoT.picker.phase != COMBO_SOARING_MAP_DONE) {
        // A cap, not the picker: nothing is chosen.
        sSoaringMapOoT.picker.phase = COMBO_SOARING_MAP_DONE;
        sSoaringMapOoT.picker.result = -1;
    }
    sSoaringMapOoT.pageOffsetY = SOARING_MAP_PAGE_HIDDEN_Y;
    sSoaringMapOoT.frames = 0;
    play->pauseCtx.state = RSBS_OOT_PAUSE_SOARING_6;
}

/** SOARING_6's end: vanilla 0x12's else branch takes the close on the next
 *  update, so SoH's PauseBufferInputs sees state 18 as on any unpause. */
static void SoaringMapHandToUnpause(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    pauseCtx->unk_1F4 = pauseCtx->unk_1F8 = pauseCtx->unk_1FC = pauseCtx->unk_200 = SOARING_MAP_ROLL_CLOSED;
    pauseCtx->alpha = 0;
    sSoaringMapOoT.closing = true;
    pauseCtx->state = RSBS_OOT_PAUSE_UNPAUSE_SETUP;
}

// ============================================================================
// The predicates the guarded arms ask (map-only RAM)
// ============================================================================

// The map states are above every vanilla state and only Begin sets them, so a
// map state alone is the map's (even with the RAM forgotten: Update then
// closes it rather than strand the pause); vanilla 0x12 is the map's only
// while its own close runs.
bool OoT_SoaringMap_CallLayerOwns(uint16_t state) {
    return state == RSBS_OOT_PAUSE_SOARING_0 || state == RSBS_OOT_PAUSE_SOARING_1;
}

bool OoT_SoaringMap_CallLayerDraws(uint16_t state) {
    return Combo_SoaringMap_DrawsState(state) || (sSoaringMapOoT.closing && state == RSBS_OOT_PAUSE_UNPAUSE_SETUP);
}

bool OoT_SoaringMap_OwnsUpdate(uint16_t state) {
    return Combo_SoaringMap_OwnsUpdate(state, sSoaringMapOoT.closing);
}

bool OoT_SoaringMap_OwnsDraw(uint16_t state) {
    return OoT_SoaringMap_OwnsUpdate(state);
}

bool OoT_SoaringMap_TakeButtonSave(void) {
    const bool take = sSoaringMapOoT.open && sSoaringMapOoT.buttonSavePending;
    sSoaringMapOoT.buttonSavePending = false;
    if (take) {
        // The map's own copy of what K3 saves into kaleido's static
        // sButtonStatusSave: the backstop, which never reaches 0x13, restores it.
        for (s32 i = 0; i < ARRAY_COUNT(gSaveContext.buttonStatus); i++) {
            sSoaringMapOoT.buttonSave[i] = gSaveContext.buttonStatus[i];
        }
        sSoaringMapOoT.buttonSaveTaken = true;
    }
    return take;
}

bool OoT_SoaringMap_IsOpen(void) {
    return sSoaringMapOoT.open;
}

int OoT_SoaringMap_CursorOwl(void) {
    return sSoaringMapOoT.open ? (int)sSoaringMapOoT.picker.cursor : -1;
}

void OoT_SoaringMapTest_Peek(int* phase, int* cursor, uint16_t* mask) {
    if (phase != NULL) {
        *phase = sSoaringMapOoT.open ? (int)sSoaringMapOoT.picker.phase : -1;
    }
    if (cursor != NULL) {
        *cursor = sSoaringMapOoT.open ? (int)sSoaringMapOoT.picker.cursor : -1;
    }
    if (mask != NULL) {
        *mask = sSoaringMapOoT.open ? sSoaringMapOoT.picker.mask : 0;
    }
}

void OoT_SoaringMap_Reset(void) {
    memset(&sSoaringMapOoT, 0, sizeof(sSoaringMapOoT));
    sSoaringMapOoT.picker.cursor = -1;
    sSoaringMapOoT.picker.result = -1;
    sSoaringMapOoT.picker.phase = COMBO_SOARING_MAP_DONE;
}

// ============================================================================
// Opening
// ============================================================================

bool OoT_SoaringMap_PauseCanOpen(PlayState* play) {
    if (play == NULL) {
        return false;
    }
    Player* player = GET_PLAYER(play);
    ComboSoaringPauseFacts f;
    memset(&f, 0, sizeof(f));
    f.pauseState = play->pauseCtx.state;
    f.debugState = play->pauseCtx.debugState;
    f.gameOverState = play->gameOverCtx.state != GAMEOVER_INACTIVE;
    f.transitionTrigger = play->transitionTrigger != TRANS_TRIGGER_OFF;
    f.transitionMode = play->transitionMode != TRANS_MODE_OFF;
    f.csState = play->csCtx.state != CS_STATE_IDLE;
    f.cutsceneIndex = gSaveContext.cutsceneIndex;
    f.nextCutsceneIndex = gSaveContext.nextCutsceneIndex;
    f.shootingGalleryStatus = play->shootingGalleryStatus;
    f.magicState = gSaveContext.magicState;
    f.gameMode = gSaveContext.gameMode;
    f.playerCsAction = player != NULL ? player->csAction : 1;
    f.playerDead = player == NULL || (player->stateFlags1 & PLAYER_STATE1_DEAD) != 0;
    f.magicStepCapacity = MAGIC_STATE_STEP_CAPACITY;
    f.magicFill = MAGIC_STATE_FILL;
    f.gameModeNormal = GAMEMODE_NORMAL;
    return Combo_SoaringMap_PauseCanOpen(&f);
}

bool OoT_SoaringMap_Begin(PlayState* play, uint16_t owlMask) {
    if (play == NULL) {
        return false;
    }
    ComboSoaringMap picker;
    if (!Combo_SoaringMap_Open(&picker, owlMask)) {
        return false;
    }
    OoT_SoaringMap_Reset();
    sSoaringMapOoT.picker = picker;
    sSoaringMapOoT.open = true;
    sSoaringMapOoT.buttonSavePending = true;
    sSoaringMapOoT.roll = SOARING_MAP_ROLL_CLOSED;
    sSoaringMapOoT.pageOffsetY = SOARING_MAP_PAGE_HIDDEN_Y;
    sSoaringMapOoT.infoOffsetY = -40; // OoTMM PrepareSoaringScreen
    sSoaringMapOoT.cursorColorTimer = 10;
    sSoaringMapOoT.clouds = Combo_SoaringCloudMask();

    // OoTMM SetupSoaring: the "You played" box goes at once.
    SoaringMapHardClearBox(play);
    play->msgCtx.textId = 0;
    play->interfaceCtx.unk_1FA = play->interfaceCtx.unk_1FC = 0;
    // MM sets OCARINA_MODE_END on entry (z_message.c); every hand-back sets it
    // again after the prompt's StartTextbox left 00.
    play->msgCtx.ocarinaMode = Combo_SoaringMap_OcarinaModeOnHandBack();
    // A backstop exit before SOARING_6 still brings the HUD back (vanilla 0x13
    // applies unk_13EE). No HUD mode is written: E1 already fades it out.
    gSaveContext.unk_13EE = RSBS_OOT_HUD_RESTORE_ALL;
    play->interfaceCtx.startAlpha = 0;
    // KaleidoSetup_Update's opening (z_kaleido_setup.c), without START.
    R_UPDATE_RATE = 2;
    if (ShrinkWindow_GetVal()) {
        ShrinkWindow_SetVal(0);
    }
    func_800F64E0(1);
    sSoaringMapOoT.soundDucked = true;
    play->pauseCtx.state = RSBS_OOT_PAUSE_SOARING_0;
    fprintf(stderr, "[OoT] Song of Soaring: the owl map opens (owls 0x%03X, cursor on owl %d) (#1169 XW8)\n",
            (unsigned)sSoaringMapOoT.picker.mask, (int)sSoaringMapOoT.picker.cursor);
    return true;
}

// ============================================================================
// K1: the call layer's waiting states (vanilla 1 and 2)
// ============================================================================

void OoT_SoaringMap_CallLayerUpdate(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    if (!sSoaringMapOoT.open) {
        // No map behind the state (unreachable: only Begin sets it): close.
        sSoaringMapOoT.open = true;
        SoaringMapEnterClose(play, COMBO_SOARING_MAP_EV_CANCEL);
        return;
    }
    sSoaringMapOoT.frames++;
    if (pauseCtx->state == RSBS_OOT_PAUSE_SOARING_0) {
        // Vanilla state 1 without the pause counter.
        if (ShrinkWindow_GetCurrentVal() == 0) {
            HREG(80) = 7;
            HREG(82) = 3;
            R_PAUSE_MENU_MODE = 1;
            pauseCtx->unk_1E4 = 0;
            pauseCtx->unk_1EC = 0;
            pauseCtx->state = RSBS_OOT_PAUSE_SOARING_1;
            sSoaringMapOoT.frames = 0;
            return;
        }
    } else if (R_PAUSE_MENU_MODE >= 3) {
        // Vanilla state 2: the prerender is ready.
        pauseCtx->state = RSBS_OOT_PAUSE_SOARING_2;
        sSoaringMapOoT.frames = 0;
        return;
    }
    if (sSoaringMapOoT.frames > RSBS_SOARING_MAP_FRAME_CAP) {
        fprintf(stderr, "[OoT] Song of Soaring: the owl map's pause never opened (state 0x%X); closing (#1169 XW8)\n",
                (unsigned)pauseCtx->state);
        SoaringMapEnterClose(play, COMBO_SOARING_MAP_EV_CANCEL);
    }
}

// ============================================================================
// K3: the update
// ============================================================================

static void SoaringMapUpdatePicker(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    ComboSoaringMap* picker = &sSoaringMapOoT.picker;
    const bool wasConfirm = picker->phase == COMBO_SOARING_MAP_CONFIRM;

    ComboSoaringMapInput in;
    memset(&in, 0, sizeof(in));
    in.dir = (int8_t)Combo_SoaringMap_Stick(picker, input->rel.stick_x, CHECK_BTN_ALL(input->press.button, BTN_DLEFT),
                                            CHECK_BTN_ALL(input->press.button, BTN_DRIGHT),
                                            CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) != 0);
    in.a = CHECK_BTN_ALL(input->press.button, BTN_A);
    in.b = CHECK_BTN_ALL(input->press.button, BTN_B);
    in.start = CHECK_BTN_ALL(input->press.button, BTN_START);
    if (wasConfirm) {
        // Whatever else would advance the box (C-up, B held under SkipText) is
        // taken here, before Message_Update could close it itself.
        in.advanceOther = !in.a && OoT_Message_ShouldAdvanceSilent(play) != 0;
        in.boxOpen = SoaringMapConfirmBoxOpen(play);
        in.choice = play->msgCtx.choiceIndex;
    }

    const ComboSoaringMapEvent ev = Combo_SoaringMap_Step(picker, &in);
    switch (ev) {
        case COMBO_SOARING_MAP_EV_MOVED:
            sSoaringMapOoT.cursorShrink = 4.0f;
            SoaringMapPlaySfx(NA_SE_SY_CURSOR);
            break;
        case COMBO_SOARING_MAP_EV_OPEN_CONFIRM:
            SoaringMapPlaySfx(NA_SE_SY_DECIDE);
            OoT_Message_StartTextbox(play, RSBS_OOT_TEXT_SOARING_CONFIRM, NULL);
            pauseCtx->state = RSBS_OOT_PAUSE_SOARING_CONFIRM;
            break;
        case COMBO_SOARING_MAP_EV_BACK:
            SoaringMapHardClearBox(play);
            SoaringMapPlaySfx(NA_SE_SY_MESSAGE_PASS);
            SoaringMapSetLabels(play);
            pauseCtx->state = RSBS_OOT_PAUSE_SOARING_SELECT;
            break;
        case COMBO_SOARING_MAP_EV_CANCEL:
        case COMBO_SOARING_MAP_EV_DECIDE:
            SoaringMapEnterClose(play, ev);
            return;
        default:
            break;
    }
    // MM runs Message_Update every frame (z_play.c); OoT skips it while paused,
    // so the prompt is advanced here, after the step took every button that
    // would advance it, and never on the frame it opened (A is still pressed).
    if (wasConfirm && picker->phase == COMBO_SOARING_MAP_CONFIRM) {
        OoT_Message_Update(play);
        // Message_Update's TEXT_STARTING frame sets A to Next (z_message_PAL.c);
        // MM's and OoTMM's owl prompt keeps Decide. A no-op while A is Decide.
        Interface_SetDoAction(play, DO_ACTION_DECIDE);
    }
}

void OoT_SoaringMap_Update(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (sSoaringMapOoT.closing && pauseCtx->state == RSBS_OOT_PAUSE_UNPAUSE_SETUP) {
        // Vanilla 0x12's else branch (z_kaleido_scope_PAL.c), taken for the
        // map's close: kaleido's head block never runs over vertices this map
        // never allocated.
        pauseCtx->debugState = 0;
        pauseCtx->state = RSBS_OOT_PAUSE_UNPAUSE_CLOSE;
        pauseCtx->unk_200 = pauseCtx->unk_1FC = pauseCtx->unk_1F8 = pauseCtx->unk_1F4 = SOARING_MAP_ROLL_CLOSED;
        pauseCtx->namedItem = PAUSE_ITEM_NONE;
        interfaceCtx->startAlpha = 0;
        return;
    }

    if (!sSoaringMapOoT.open) {
        // No map behind the state (unreachable: only Begin sets it): close.
        sSoaringMapOoT.open = true;
        SoaringMapEnterClose(play, COMBO_SOARING_MAP_EV_CANCEL);
        return;
    }
    sSoaringMapOoT.frames++;
    switch (pauseCtx->state) {
        case RSBS_OOT_PAUSE_SOARING_2:
            SoaringMapSetLabels(play);
            sSoaringMapOoT.roll = SOARING_MAP_ROLL_CLOSED;
            sSoaringMapOoT.pageOffsetY = SOARING_MAP_PAGE_HIDDEN_Y;
            sSoaringMapOoT.frames = 0;
            pauseCtx->state = RSBS_OOT_PAUSE_SOARING_3;
            return;

        case RSBS_OOT_PAUSE_SOARING_3:
            // MM OWL_WARP_3 (z_kaleido_scope_NES.c).
            sSoaringMapOoT.owlAlpha += 20;
            sSoaringMapOoT.infoOffsetY += 10;
            sSoaringMapOoT.roll -= 40.0f;
            interfaceCtx->startAlpha += 63;
            sSoaringMapOoT.alpha += 31;
            if (sSoaringMapOoT.roll <= 0.0f) {
                sSoaringMapOoT.roll = 0.0f;
                interfaceCtx->startAlpha = 255;
                sSoaringMapOoT.pageOffsetY = 0.0f;
                sSoaringMapOoT.alpha = 255;
                sSoaringMapOoT.owlAlpha = 120;
                sSoaringMapOoT.frames = 0;
                pauseCtx->state = RSBS_OOT_PAUSE_SOARING_SELECT;
            } else if (sSoaringMapOoT.frames > RSBS_SOARING_MAP_FRAME_CAP) {
                SoaringMapEnterClose(play, COMBO_SOARING_MAP_EV_CANCEL);
            }
            return;

        case RSBS_OOT_PAUSE_SOARING_SELECT:
        case RSBS_OOT_PAUSE_SOARING_CONFIRM:
            // The player is in control: no cap.
            SoaringMapUpdatePicker(play);
            return;

        case RSBS_OOT_PAUSE_SOARING_6:
            // MM OWL_WARP_6.
            if (sSoaringMapOoT.roll < SOARING_MAP_ROLL_CLOSED && sSoaringMapOoT.frames <= RSBS_SOARING_MAP_FRAME_CAP) {
                sSoaringMapOoT.owlAlpha -= 60;
                if (sSoaringMapOoT.owlAlpha < 0) {
                    sSoaringMapOoT.owlAlpha = 0;
                }
                sSoaringMapOoT.infoOffsetY -= 10;
                sSoaringMapOoT.roll += 40.0f;
                interfaceCtx->startAlpha -= 63;
                if (interfaceCtx->startAlpha < 0) {
                    interfaceCtx->startAlpha = 0;
                }
                sSoaringMapOoT.alpha -= 63;
                if (sSoaringMapOoT.alpha < 0 || sSoaringMapOoT.roll >= SOARING_MAP_ROLL_CLOSED) {
                    sSoaringMapOoT.alpha = 0;
                }
                if (sSoaringMapOoT.roll > SOARING_MAP_ROLL_CLOSED) {
                    sSoaringMapOoT.roll = SOARING_MAP_ROLL_CLOSED;
                }
                return;
            }
            SoaringMapHandToUnpause(play);
            return;

        default:
            // SOARING_0 / _1 belong to the call layer (K1); anything else is
            // not a state this map sets. Close rather than strand the pause.
            SoaringMapEnterClose(play, COMBO_SOARING_MAP_EV_CANCEL);
            return;
    }
}

// ============================================================================
// The close's end, and the backstop
// ============================================================================

void OoT_SoaringMap_OnKaleidoscopeUpdate(PlayState* play) {
    if (play == NULL || !sSoaringMapOoT.open || !sSoaringMapOoT.closing || play->pauseCtx.state != 0) {
        return;
    }
    const int owl = sSoaringMapOoT.picker.result;
    OoT_SoaringMap_Reset();
    if (play->msgCtx.textId == RSBS_OOT_TEXT_SOARING_CONFIRM) {
        SoaringMapHardClearBox(play);
    }
    if (owl >= 0) {
        // In the frame the pause state turned 0: the transition it starts is
        // set before the next KaleidoSetup_Update, which then refuses START.
        OoT_SoaringOoT_Depart(play, owl);
        return;
    }
    play->msgCtx.ocarinaMode = Combo_SoaringMap_OcarinaModeOnHandBack();
}

void OoT_SoaringMap_Backstop(PlayState* play) {
    if (play == NULL || !sSoaringMapOoT.open) {
        return;
    }
    const uint16_t state = play->pauseCtx.state;
    if (Combo_SoaringMap_IsMapState(state) || state == RSBS_OOT_PAUSE_UNPAUSE_SETUP ||
        state == RSBS_OOT_PAUSE_UNPAUSE_CLOSE) {
        return;
    }
    // Something else closed (or replaced) the pause: Link gets control back.
    const bool soundDucked = sSoaringMapOoT.soundDucked;
    const bool buttonSaveTaken = sSoaringMapOoT.buttonSaveTaken;
    u8 buttonSave[ARRAY_COUNT(gSaveContext.buttonStatus)];
    memcpy(buttonSave, sSoaringMapOoT.buttonSave, sizeof(buttonSave));
    OoT_SoaringMap_Reset();
    if (state == 0) {
        // What vanilla 0x13 undoes (z_kaleido_scope_PAL.c) and this path never
        // reaches: Begin's update rate and sound duck, the call layer's menu
        // mode, the labels and the buttons. A vanilla pause that replaced the
        // map is left alone: its own 0x13 restores them.
        R_UPDATE_RATE = 3;
        R_PAUSE_MENU_MODE = 0;
        if (soundDucked) {
            func_800F64E0(0);
        }
        play->interfaceCtx.unk_1FA = play->interfaceCtx.unk_1FC = 0;
        Interface_SetDoAction(play, DO_ACTION_NONE);
        if (buttonSaveTaken) {
            for (s32 i = 0; i < ARRAY_COUNT(gSaveContext.buttonStatus); i++) {
                gSaveContext.buttonStatus[i] = buttonSave[i];
            }
        }
    }
    if (play->msgCtx.textId == RSBS_OOT_TEXT_SOARING_CONFIRM) {
        SoaringMapHardClearBox(play);
    }
    play->msgCtx.ocarinaMode = Combo_SoaringMap_OcarinaModeOnHandBack();
    if (gSaveContext.unk_13EA == 1) {
        Interface_ChangeAlpha(RSBS_OOT_HUD_RESTORE_ALL);
    }
}

// ============================================================================
// K4: the draw (MM's owl-warp page, OoTMM's port into OoT's renderer)
// ============================================================================

static void SoaringMapSetQuad(Vtx* v, s16 x, s16 y, s16 w, s16 h, s16 tw, s16 th, u8 shade, u8 alpha) {
    s32 i;
    for (i = 0; i < 4; i++) {
        v[i].v.ob[0] = (i & 1) ? (s16)(x + w) : x;
        v[i].v.ob[1] = (i & 2) ? (s16)(y - h) : y;
        v[i].v.ob[2] = 0;
        v[i].v.flag = 0;
        v[i].v.tc[0] = (i & 1) ? (s16)(tw << 5) : 0;
        v[i].v.tc[1] = (i & 2) ? (s16)(th << 5) : 0;
        v[i].v.cn[0] = v[i].v.cn[1] = v[i].v.cn[2] = shade;
        v[i].v.cn[3] = alpha;
    }
}

static u8 SoaringMapAlpha8(s16 alpha) {
    return (u8)(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
}

static bool SoaringMapRolling(uint16_t state) {
    // MM KaleidoScope_SetVertices: offsetY 80 while the page rolls.
    return state == RSBS_OOT_PAUSE_SOARING_2 || state == RSBS_OOT_PAUSE_SOARING_3 || state == RSBS_OOT_PAUSE_SOARING_6;
}

static Vtx* SoaringMapPageVertices(PlayState* play) {
    Vtx* vtx = Graph_Alloc(play->state.gfxCtx, (SOARING_MAP_PAGE_QUADS * 4) * sizeof(Vtx));
    const s16 offsetY = SoaringMapRolling(play->pauseCtx.state) ? 80 : 0;
    const u8 alpha = SoaringMapAlpha8(sSoaringMapOoT.alpha);
    ComboSoaringQuad q;
    s32 k = 0;
    s32 i;
    s32 j;

    // MM KaleidoScope_SetPageVertices: three columns of five 80x32 quads.
    for (i = 0; i < 3; i++) {
        const s16 x = (s16)(-120 + i * 80);
        for (j = 0; j < 5; j++, k++) {
            SoaringMapSetQuad(&vtx[k * 4], x, (s16)(80 - j * 32 + offsetY), 80, 32, 80, 32, 0, alpha);
        }
    }
    for (i = 0; i < RSBS_SOARING_CLOUD_COUNT; i++, k++) {
        Combo_SoaringMap_CloudQuad(i, &q);
        SoaringMapSetQuad(&vtx[k * 4], q.x, (s16)(q.y + offsetY), q.w, q.h, q.w, q.h, 255, alpha);
    }
    for (i = 0; i < RSBS_SOARING_OWL_COUNT; i++, k++) {
        Combo_SoaringMap_OwlQuad(i, &q);
        SoaringMapSetQuad(&vtx[k * 4], q.x, (s16)(q.y + offsetY), q.w, q.h, q.w, q.h, 255, alpha);
    }
    // The world map image: 9-row fragments from y 59 down, the last 2 rows high.
    for (i = 0; i < SOARING_MAP_FRAG_NUM; i++, k++) {
        const s16 h = (i == SOARING_MAP_FRAG_NUM - 1) ? (SOARING_MAP_IMAGE_H % SOARING_MAP_FRAG_H) : SOARING_MAP_FRAG_H;
        SoaringMapSetQuad(&vtx[k * 4], -(SOARING_MAP_IMAGE_W / 2) - 1, (s16)(59 - i * SOARING_MAP_FRAG_H + offsetY),
                          SOARING_MAP_IMAGE_W, h, SOARING_MAP_IMAGE_W, h, alpha, alpha);
    }
    return vtx;
}

static void SoaringMapDrawWorldMap(PlayState* play, Vtx* vtx) {
    const u8 alpha = SoaringMapAlpha8(sSoaringMapOoT.alpha);
    const char* image = Combo_SoaringMap_WorldImageTex();
    s32 i;
    s32 j;
    s32 k = 0;

    OPEN_DISPS(play->state.gfxCtx);

    // MM_KaleidoScope_DrawWorldMap's angled branch, the CI8 image by path with
    // LoadMultiTile as OoT's own world map loads its own (z_kaleido_map_PAL.c).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPLoadTLUT_pal256(POLY_OPA_DISP++, Combo_SoaringMap_WorldImageTlut());
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_RGBA16);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, alpha);

    gSPVertex(POLY_OPA_DISP++, &vtx[SOARING_MAP_QUAD_IMAGE0 * 4], 8 * 4, 0);
    for (i = 0, j = 0; i < 8; i++, k++, j += 4) {
        gDPLoadMultiTile(POLY_OPA_DISP++, image, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, SOARING_MAP_IMAGE_W,
                         SOARING_MAP_IMAGE_H, 0, k * SOARING_MAP_FRAG_H, SOARING_MAP_IMAGE_W - 1,
                         (k + 1) * SOARING_MAP_FRAG_H - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetTileSize(POLY_OPA_DISP++, G_TX_RENDERTILE, 0, 0, (SOARING_MAP_IMAGE_W - 1) << G_TEXTURE_IMAGE_FRAC,
                       (SOARING_MAP_FRAG_H - 1) << G_TEXTURE_IMAGE_FRAC);
        gSP1Quadrangle(POLY_OPA_DISP++, j, j + 2, j + 3, j + 1, 0);
    }
    gSPVertex(POLY_OPA_DISP++, &vtx[(SOARING_MAP_QUAD_IMAGE0 + 8) * 4], (6 + 1) * 4, 0);
    for (i = 0, j = 0; i < 6; i++, k++, j += 4) {
        gDPLoadMultiTile(POLY_OPA_DISP++, image, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, SOARING_MAP_IMAGE_W,
                         SOARING_MAP_IMAGE_H, 0, k * SOARING_MAP_FRAG_H, SOARING_MAP_IMAGE_W - 1,
                         (k + 1) * SOARING_MAP_FRAG_H - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPSetTileSize(POLY_OPA_DISP++, G_TX_RENDERTILE, 0, 0, (SOARING_MAP_IMAGE_W - 1) << G_TEXTURE_IMAGE_FRAC,
                       (SOARING_MAP_FRAG_H - 1) << G_TEXTURE_IMAGE_FRAC);
        gSP1Quadrangle(POLY_OPA_DISP++, j, j + 2, j + 3, j + 1, 0);
    }
    gDPLoadMultiTile(POLY_OPA_DISP++, image, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, SOARING_MAP_IMAGE_W,
                     SOARING_MAP_IMAGE_H, 0, k * SOARING_MAP_FRAG_H, SOARING_MAP_IMAGE_W - 1,
                     k * SOARING_MAP_FRAG_H + (SOARING_MAP_IMAGE_H % SOARING_MAP_FRAG_H) - 1, 0,
                     G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                     G_TX_NOLOD);
    gDPSetTileSize(POLY_OPA_DISP++, G_TX_RENDERTILE, 0, 0, (SOARING_MAP_IMAGE_W - 1) << G_TEXTURE_IMAGE_FRAC,
                   ((SOARING_MAP_IMAGE_H % SOARING_MAP_FRAG_H) - 1) << G_TEXTURE_IMAGE_FRAC);
    gSP1Quadrangle(POLY_OPA_DISP++, j, j + 2, j + 3, j + 1, 0);

    // The clouds MM's frozen half has not cleared.
    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_BILERP);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 215, 235, 235, alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 40, 60, 100, 0);
    for (i = 0; i < RSBS_SOARING_CLOUD_COUNT; i++) {
        ComboSoaringQuad q;
        if ((sSoaringMapOoT.clouds >> i) & 1u) {
            continue;
        }
        Combo_SoaringMap_CloudQuad(i, &q);
        gSPVertex(POLY_OPA_DISP++, &vtx[(SOARING_MAP_QUAD_CLOUD0 + i) * 4], 4, 0);
        gDPLoadTextureBlock(POLY_OPA_DISP++, Combo_SoaringMap_CloudTex(i), G_IM_FMT_IA, G_IM_SIZ_8b, q.w, q.h, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                            G_TX_NOLOD);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    }

    // The darkening rectangle (MM: owl-warp states only, which these all are).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, SoaringMapAlpha8(sSoaringMapOoT.owlAlpha));
    gDPFillRectangle(POLY_OPA_DISP++, 50, 62, 270, 190);

    // The owl face on every active statue, and only there.
    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPLoadTextureBlock(POLY_OPA_DISP++, Combo_SoaringMap_OwlFaceTex(), G_IM_FMT_RGBA, G_IM_SIZ_32b, 24, 12, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, alpha);
    for (i = 0; i < RSBS_SOARING_OWL_COUNT; i++) {
        if ((sSoaringMapOoT.picker.mask >> i) & 1u) {
            gSPVertex(POLY_OPA_DISP++, &vtx[(SOARING_MAP_QUAD_OWL0 + i) * 4], 4, 0);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        }
    }
    gDPPipeSync(POLY_OPA_DISP++);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void SoaringMapDrawPage(PlayState* play) {
    // OoT's map-page frame (its ENG list) with MM's map title as column 1, row 0
    // (OoTMM: sMapPageBgTextures[5] = MM's gPauseMap10ENGTex).
    void* texs[SOARING_MAP_BG_QUADS] = {
        (void*)gPauseMap00Tex, (void*)gPauseMap01Tex, (void*)gPauseMap02Tex,
        (void*)gPauseMap03Tex, (void*)gPauseMap04Tex, NULL,
        (void*)gPauseMap11Tex, (void*)gPauseMap12Tex, (void*)gPauseMap13Tex,
        (void*)gPauseMap14Tex, (void*)gPauseMap20Tex, (void*)gPauseMap21Tex,
        (void*)gPauseMap22Tex, (void*)gPauseMap23Tex, (void*)gPauseMap24Tex,
    };
    Vtx* vtx = SoaringMapPageVertices(play);
    texs[5] = (void*)Combo_SoaringMap_TitleTex();

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineLERP(POLY_OPA_DISP++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
                      SHADE, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 180, 180, 120, 255);

    // OoT's own map-page matrix (z_kaleido_scope_PAL.c, DrawPages), with the
    // map's own roll and vertical offset (OoTMM KaleidoScope_DrawOwlWarpMapPage).
    OoT_Matrix_Translate((f32)WREG(3) / 100.0f, sSoaringMapOoT.pageOffsetY / 100.0f, 0.0f, MTXMODE_NEW);
    OoT_Matrix_Scale(0.78f, 0.78f, 0.78f, MTXMODE_APPLY);
    Matrix_RotateZ(-sSoaringMapOoT.roll / 100.0f, MTXMODE_APPLY);
    Matrix_RotateY(-1.57f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    POLY_OPA_DISP = OoT_KaleidoScope_DrawPageSections(POLY_OPA_DISP, vtx, texs);

    CLOSE_DISPS(play->state.gfxCtx);

    SoaringMapDrawWorldMap(play, vtx);
}

static void SoaringMapDrawInfoPanel(PlayState* play) {
    Vtx* vtx = Graph_Alloc(play->state.gfxCtx, SOARING_MAP_PANEL_VTX * sizeof(Vtx));
    const u8 alpha = SoaringMapAlpha8(sSoaringMapOoT.alpha);
    const s16 y = (s16)(sSoaringMapOoT.infoOffsetY - 76);

    // MM KaleidoScope_DrawOwlWarpInfoPanel: the two panel halves only. MM's
    // owl-warp panel draws gItemNamePanelDL and no page arrows (OoT draws its
    // arrows with gLButtonIconDL / gRButtonIconDL, which this panel never
    // calls). Vertices 8..15 are zeroed so the 16-vertex load MM and OoT make
    // reads defined data.
    SoaringMapSetQuad(&vtx[0], -72, y, 72, 24, 72, 24, 200, alpha);
    SoaringMapSetQuad(&vtx[4], 0, y, 72, 24, 72, 24, 200, alpha);
    memset(&vtx[8], 0, 8 * sizeof(Vtx));
    SoaringMapSetQuad(&vtx[16], -63, (s16)(sSoaringMapOoT.infoOffsetY - 80), 128, 16, 128, 16, 255, 255);

    OPEN_DISPS(play->state.gfxCtx);

    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    OoT_Matrix_Translate(0.0f, 0.0f, -144.0f, MTXMODE_NEW);
    OoT_Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 150, 140, 90, 255);
    gSPVertex(POLY_OPA_DISP++, &vtx[0], 16, 0);
    gSPDisplayList(POLY_OPA_DISP++, gItemNamePanelDL);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetEnvColor(POLY_OPA_DISP++, 20, 30, 40, 0);
    if (play->pauseCtx.state == RSBS_OOT_PAUSE_SOARING_SELECT) {
        // MM's own name texture, shown steadily (the map page's timer stays 0).
        const char* name = Combo_SoaringMap_OwlNameTex(sSoaringMapOoT.picker.cursor);
        if (name != NULL) {
            gSPVertex(POLY_OPA_DISP++, &vtx[16], 4, 0);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
            gDPLoadTextureBlock_4b(POLY_OPA_DISP++, name, G_IM_FMT_IA, 128, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static s16 SoaringMapStepColor(s16 cur, s16 target, s16 timer) {
    const s16 step = (s16)(ABS(cur - target) / timer);
    return (s16)(cur >= target ? cur - step : cur + step);
}

static void SoaringMapDrawCursor(PlayState* play) {
    // OoT's four corner textures at MM's cursor position, unspun, in MM's BLUE
    // set (OoTMM KaleidoScope_DrawCursor / UpdateCursorSize).
    static void* sCorners[4];
    static s16 sPrevOwl = -1;
    static u8 sInterpState = 0;
    const s16 owl = sSoaringMapOoT.picker.cursor;
    const s16 timer = sSoaringMapOoT.cursorColorTimer > 0 ? sSoaringMapOoT.cursorColorTimer : 1;
    const s16 set = sSoaringMapOoT.cursorColorIndex & 1;
    f32 cx = 0.0f;
    f32 cy = 0.0f;
    f32 size;
    s32 i;

    sCorners[0] = (void*)gPauseMenuCursorTopLeftTex;
    sCorners[1] = (void*)gPauseMenuCursorTopRightTex;
    sCorners[2] = (void*)gPauseMenuCursorBottomRightTex;
    sCorners[3] = (void*)gPauseMenuCursorBottomLeftTex;

    for (i = 0; i < 3; i++) {
        sSoaringMapOoT.cursorPrim[i] =
            SoaringMapStepColor(sSoaringMapOoT.cursorPrim[i], kSoaringMapCursorPrim[set][i], timer);
        sSoaringMapOoT.cursorEnv[i] =
            SoaringMapStepColor(sSoaringMapOoT.cursorEnv[i], kSoaringMapCursorEnv[set][i], timer);
    }
    sSoaringMapOoT.cursorColorTimer--;
    if (sSoaringMapOoT.cursorColorTimer <= 0) {
        for (i = 0; i < 3; i++) {
            sSoaringMapOoT.cursorPrim[i] = kSoaringMapCursorPrim[set][i];
            sSoaringMapOoT.cursorEnv[i] = kSoaringMapCursorEnv[set][i];
        }
        sSoaringMapOoT.cursorColorIndex ^= 1;
        sSoaringMapOoT.cursorColorTimer = 10;
    }

    if (!Combo_SoaringMap_OwlCursor(owl, &cx, &cy)) {
        return;
    }
    size = 7.5f - sSoaringMapOoT.cursorShrink;
    if (size < 0.0f) {
        size = 0.0f;
    }
    if (sSoaringMapOoT.cursorShrink > 0.0f) {
        sSoaringMapOoT.cursorShrink -= 1.0f;
    }
    // MM: the cursor skips interpolation for the frame it moves.
    if (owl != sPrevOwl) {
        sInterpState ^= 1;
        sPrevOwl = owl;
    }

    Vtx* vtx = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
    SoaringMapSetQuad(vtx, -8, 8, 16, 16, 16, 16, 255, 255);

    OPEN_DISPS(play->state.gfxCtx);

    FrameInterpolation_RecordOpenChild("Soaring map cursor", sInterpState);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, sSoaringMapOoT.cursorPrim[0], sSoaringMapOoT.cursorPrim[1],
                    sSoaringMapOoT.cursorPrim[2], 255);
    gDPSetEnvColor(POLY_OPA_DISP++, sSoaringMapOoT.cursorEnv[0], sSoaringMapOoT.cursorEnv[1],
                   sSoaringMapOoT.cursorEnv[2], 255);
    OoT_Matrix_Translate(cx - 4.0f, cy + 4.0f, -50.0f, MTXMODE_NEW);
    OoT_Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    for (i = 0; i < 4; i++) {
        const s16 phase = (s16)(0xE000 + i * 0x4000);
        OoT_Matrix_Push();
        OoT_Matrix_Translate(OoT_Math_SinS(phase) * size, OoT_Math_CosS(phase) * size, -50.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPLoadTextureBlock_4b(POLY_OPA_DISP++, sCorners[i], G_IM_FMT_IA, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(POLY_OPA_DISP++, vtx, 4, 0);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        OoT_Matrix_Pop();
    }
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    FrameInterpolation_RecordCloseChild();

    CLOSE_DISPS(play->state.gfxCtx);
}

void OoT_SoaringMap_Draw(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (pauseCtx->state != RSBS_OOT_PAUSE_UNPAUSE_SETUP) {
        // OoTMM KaleidoScope_BeforeDraw, with the eye MM's func_800F4A10 sets.
        OoT_KaleidoScope_SetView(pauseCtx, -64.0f, 0.0f, 0.0f);
        Gfx_SetupDL_42Opa(play->state.gfxCtx);
        SoaringMapDrawPage(play);

        Gfx_SetupDL_42Opa(play->state.gfxCtx);
        OPEN_DISPS(play->state.gfxCtx);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        CLOSE_DISPS(play->state.gfxCtx);

        OoT_KaleidoScope_SetView(pauseCtx, 0.0f, 0.0f, 64.0f);
        SoaringMapDrawInfoPanel(play);
        if (pauseCtx->state == RSBS_OOT_PAUSE_SOARING_SELECT) {
            SoaringMapDrawCursor(play);
        }
    }
    // The 0x12 frame of the close draws nothing of the pause; every frame puts
    // the play view back, as the end of OoT_KaleidoScope_Draw does.
    func_800AAA50(&play->view, 15);
}

#endif // RSBS_SINGLE_EXECUTABLE
