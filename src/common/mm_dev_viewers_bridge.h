#ifndef RSBS_COMMON_MM_DEV_VIEWERS_BRIDGE_H
#define RSBS_COMMON_MM_DEV_VIEWERS_BRIDGE_H

/*
 * Single-executable bridge from SoH's hosted W6-DEV windows to 2Ship's DeveloperTools bodies.
 * Includes only C-compatible primitives and forward-declared types (no MM headers).
 * Slice 1 (#1238): Message Viewer. Slice 2 (#1238): Collision Viewer.
 */

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Returns true while MM's Message Viewer body is live and SoH's Message Viewer window should host it. */
bool MM_MessageViewer_HostedActive(void);

/** Draws MM's hosted Message Viewer element inside SoH's Message Viewer frame (must be called under ImGui frame). */
void MM_MessageViewer_DrawHostedElement(void);

/** Updates MM's hosted Message Viewer element (lazy-inits the body on first call while MM is active). */
void MM_MessageViewer_UpdateHostedElement(void);

/** Test only: returns whether the hosted MM Message Viewer body has been initialized (never initializes it). */
bool MM_MessageViewer_HostedInitializedForTest(void);

/** Returns true when SoH's Collision Viewer window should host Majora's Mask's Collision Viewer body. */
bool MM_CollisionViewer_HostedActive(void);
/** Draws Majora's Mask's Collision Viewer body inside SoH's Collision Viewer window (call only while HostedActive). */
void MM_CollisionViewer_DrawHostedElement(void);
/** Initializes MM's Collision Viewer body and updates it on the first MM frame, whether the window is open or not. */
void MM_CollisionViewer_UpdateHostedElement(void);
/** Test only: returns whether the hosted MM Collision Viewer body has been initialized (never initializes it). */
bool MM_CollisionViewer_HostedInitializedForTest(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_MM_DEV_VIEWERS_BRIDGE_H
