#pragma once

#ifdef __cplusplus
#include <ship/window/gui/GuiWindow.h>

#ifdef RSBS_SINGLE_EXECUTABLE
namespace S2H {
#else
extern "C" {
#endif
#endif

void DrawCollisionViewer();

#ifdef __cplusplus
#ifndef RSBS_SINGLE_EXECUTABLE
}
#endif

class CollisionViewerWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override{};
};

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H
#endif

#endif
