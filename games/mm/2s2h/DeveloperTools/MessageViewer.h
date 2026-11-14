#pragma once

#include "z64.h"

#ifdef __cplusplus
#include <ship/window/gui/GuiWindow.h>
#include <unordered_map>

#ifdef RSBS_SINGLE_EXECUTABLE
// Single-executable builds compile both OoT and MM into one binary; SoH's
// games/oot/soh/Enhancements/debugger/MessageViewer.h already declares
// extern "C" MessageDebug_StartTextBox / MessageDebug_DisplayCustomMessage
// and class ::MessageViewer, so isolate MM's declarations in namespace S2H
// with C++ linkage to prevent C linkage / ODR collisions.
namespace S2H {
#else
extern "C" {
#endif // RSBS_SINGLE_EXECUTABLE
#endif // __cplusplus

/**
 * \brief Displays a vanilla message in a text box on screen.
 * \param tableId Unused (reserved for future use)
 * \param textId The textId corresponding to the message to display
 * \param language Unused (reserved for future use)
 */
void MessageDebug_StartTextBox(const char* tableId, uint16_t textId, uint8_t language);

/**
 * \brief Displays a custom message using Custom Message Syntax.
 * \param customMessage A string using Custom Message Syntax.
 */
void MessageDebug_DisplayCustomMessage(const char* customMessage);

#ifdef __cplusplus
#ifndef RSBS_SINGLE_EXECUTABLE
}
#endif

class MessageViewerWindow : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;

    ~MessageViewerWindow() override;

  private:
    void DisplayExistingMessage() const;
    void DisplayCustomMessage() const;
    void LoadMessageToEditor();
    bool ParseTextIdFromBuffer(uint16_t& outTextId);

    static constexpr uint16_t MAX_STRING_SIZE = 1024;
    static constexpr int HEXADECIMAL = 0;
    static constexpr int DECIMAL = 1;

    char* mTextIdBuf;
    uint16_t mTextId;
    int mTextIdBase = HEXADECIMAL;
    char* mCustomMessageBuf;
    std::string mCustomMessageString;
    bool mDisplayExistingMessageClicked = false;
    bool mDisplayCustomMessageClicked = false;
    bool mLoadMessageClicked = false;
};

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H
#endif

#endif // __cplusplus
