#ifndef AUTHENTIC_GFX_PATCHES_H
#define AUTHENTIC_GFX_PATCHES_H

void GfxPatcher_ApplyNecessaryAuthenticPatches();
void GfxPatcher_ApplyOverflowTexturePatches();
void GfxPatcher_ApplyGeometryIssuePatches();
#ifdef RSBS_SINGLE_EXECUTABLE
// RSBS (#1811): MmGraphicsSingleExe.cpp's keyed reapply runs it under MM's resource cache owner.
void PatchCircleShadowStreaks();
#endif

#endif // AUTHENTIC_GFX_PATCHES_H
