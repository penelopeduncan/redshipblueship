#pragma once

#include <libultraship/libultraship.h>

typedef enum { COLVIEW_DISABLED, COLVIEW_SOLID, COLVIEW_TRANSPARENT } ColViewerRenderSetting;

#ifdef __cplusplus
// Wrapped by OoTGatedHostingMm (#1238).
#ifdef RSBS_SINGLE_EXECUTABLE
class ColViewerWindow : public Ship::GuiWindow {
#else
class ColViewerWindow final : public Ship::GuiWindow {
#endif
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override{};
};

#endif
