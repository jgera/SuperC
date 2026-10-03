#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <functional>

// Copies a wide string to Windows clipboard
bool CopyToClipboard(const std::wstring& text);

// Gets current text from Windows clipboard
std::wstring GetClipboardText();

struct DialogConfig {
    std::wstring title;
    std::wstring prompt;
    std::wstring resultText;
    bool autoCopied = true;
    bool multiline = false;
    
    // Optional secondary button (e.g., "Kill Process")
    std::wstring secondaryButtonText;
    std::function<void()> onSecondaryAction;
};

// Shows a native, clean result dialog centered on the screen.
// Pressing Enter copies & closes. Pressing Esc closes.
void ShowResultDialog(const DialogConfig& config);

// Shows the full cheat sheet / help dialog
void ShowHelpDialog();
