#pragma once
/**
 * @file SoaringMapOoT.h
 * @brief The Termina owl map in Ocarina of Time's pause (#1169 lane XW8). The
 *        decisions are src/common/warp_song_soaring_map.h; SoaringMapOoT.c acts
 *        on them, and the guarded kaleido arms call what is declared here:
 *          K1  z_kaleido_scope_call.c, KaleidoScopeCall_Update: SOARING_0 and _1
 *          K2  z_kaleido_scope_call.c, KaleidoScopeCall_Draw: the draw gate
 *          K3  z_kaleido_scope_PAL.c, KaleidoScope_Update: the map's update
 *          K4  z_kaleido_scope_PAL.c, KaleidoScope_Draw: the map's draw
 *        Every predicate reads map-only RAM, so OoT's pause behaves exactly as
 *        before whenever no map is open.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdbool.h>
#include <stdint.h>

struct PlayState;

#ifdef __cplusplus
extern "C" {
#endif

/** K1: the call layer's two waiting states (vanilla 1 and 2 for the map). */
bool OoT_SoaringMap_CallLayerOwns(uint16_t state);
void OoT_SoaringMap_CallLayerUpdate(struct PlayState* play);
/** K2: whether the call layer draws @p state for the map. */
bool OoT_SoaringMap_CallLayerDraws(uint16_t state);
/** K3: whether KaleidoScope_Update hands @p state to the map (a map state, or
 *  the one vanilla 0x12 frame of the map's own close). */
bool OoT_SoaringMap_OwnsUpdate(uint16_t state);
/** K3: true once per map open, on its first kaleido update: the caller copies
 *  gSaveContext.buttonStatus into kaleido's sButtonStatusSave, which vanilla
 *  0x13 puts back. */
bool OoT_SoaringMap_TakeButtonSave(void);
void OoT_SoaringMap_Update(struct PlayState* play);
/** K4: whether KaleidoScope_Draw draws the map instead (the same states). */
bool OoT_SoaringMap_OwnsDraw(uint16_t state);
void OoT_SoaringMap_Draw(struct PlayState* play);

/** Whether a pause may open now (Combo_SoaringMap_PauseCanOpen over this
 *  frame's facts; never OoT_Play_InCsMode, which the ocarina itself sets). */
bool OoT_SoaringMap_PauseCanOpen(struct PlayState* play);
/** Opens the map over @p owlMask (OoTMM SetupSoaring). False, with nothing
 *  changed, when no owl is active. */
bool OoT_SoaringMap_Begin(struct PlayState* play, uint16_t owlMask);

/** Whether a map is open or closing (RAM only). */
bool OoT_SoaringMap_IsOpen(void);
/** OnKaleidoscopeUpdate: the close's end (state 0): Depart or hand back. */
void OoT_SoaringMap_OnKaleidoscopeUpdate(struct PlayState* play);
/** OnPlayerUpdate: a map marked open whose pause state is no longer its own
 *  (something else closed the pause): hand control back. */
void OoT_SoaringMap_Backstop(struct PlayState* play);
/** OnLoadGame and OnPlayDestroy: forget the map. */
void OoT_SoaringMap_Reset(void);
/** The owl under the cursor, for the confirm prompt's text (-1 when closed). */
int OoT_SoaringMap_CursorOwl(void);

/** SongOfSoaringOoT.cpp: every MM resource the map draws is mounted. */
bool OoT_SoaringMap_AssetsPresent(void);

/** Test seams (int-soaring-to-owl): the picker as it stands, and a probe
 *  override that makes the assets read absent (1) or real (0). */
void OoT_SoaringMapTest_Peek(int* phase, int* cursor, uint16_t* mask);
void OoT_SoaringMapTest_ForceAssetsAbsent(int absent);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
