#pragma once

#include <cstddef>
#include <ship/window/gui/GuiWindow.h>

class EventLogWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override{};
};

#ifdef RSBS_SINGLE_EXECUTABLE
/**
 * Returns the number of currently buffered event log entries and clears them.
 * Exposed for MMDevViewersGate unit tests.
 */
size_t MM_EventLog_DrainEntryCountForTest();
#endif
