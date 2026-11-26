#pragma once

void ApplyAuthenticGfxPatches();
void ApplyMirrorWorldGfxPatches();

#ifdef RSBS_SINGLE_EXECUTABLE
// RSBS (#1817): the owner-gated reapply of ApplyAuthenticGfxPatches (authenticGfxPatches.cpp). Patches at once while
// OoT runs and returns 1; otherwise owes the pass and returns 0. ApplyOwed pays an owed pass while OoT runs.
extern "C" int OoT_AuthenticGfxPatches_Reapply(void);
extern "C" int OoT_AuthenticGfxPatches_ApplyOwed(void);
#endif
