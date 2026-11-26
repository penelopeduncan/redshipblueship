#pragma once

#ifdef RSBS_SINGLE_EXECUTABLE
// RSBS (#1829): the owner gate for OoT's display-list registrars (Fix Hammer Hand, Equipment Always Visible, Temple of
// Time medallion colors, Colored Maps and Compasses, Mirrored World). ResourceMgr_PatchGfxByName and its unpatch work
// on the copy of a list that the current resource cache owner holds (#981), and while Majora's Mask runs that is MM's
// owner, so a pass a row click, a preset or a dropped config asks for then is owed to OoT instead and paid on OoT's
// next frame (CosmeticsUpdateTick). The same shape as OoT_AuthenticGfxPatches_Reapply (#1817), for several passes.

// A registrar's patch/unpatch body. `owed` is true only when the payment calls it.
using OoTGfxPatchPass = void (*)(bool owed);

// While OoT runs: calls pass(false) at once, which is the registrar's own body. Otherwise owes it under `id` (a string
// literal), once per id, in the order of the first request. `needsPlay` marks a pass that reads OoT's save: it is paid
// only on a frame with a play state, because OoT's first frame after a return from MM is the Opening gamestate, whose
// debug save stands in for OoT's own until Play_Init restores it.
void OoT_GfxPatches_RunOrOwe(const char* id, OoTGfxPatchPass pass, bool needsPlay);

// Pays what is owed while OoT runs (each pass at most once) and returns how many passes it made.
extern "C" int OoT_GfxPatches_ApplyOwed(void);
#endif
