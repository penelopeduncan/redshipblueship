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

/**
 * Returns the type name ("FlagUnset", "SceneFlagUnset", ...) of the newest
 * buffered event log entry, or nullptr when the log is empty. Does not clear.
 * Exposed for MMDevViewersGate unit tests.
 */
const char* MM_EventLog_NewestEntryTypeNameForTest();
#endif
