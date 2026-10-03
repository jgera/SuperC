#pragma once
#include <windows.h>
#include <string>

// Initializes and creates the floating Quick Launcher window
HWND CreateLauncherWindow(HINSTANCE hInstance);

// Shows the launcher centered and focused
void ShowLauncher();

// Hides the launcher and clears search state
void HideLauncher();

// Toggles launcher visibility
void ToggleLauncher();

// Checks if launcher is currently visible
bool IsLauncherVisible();

// Startup toggle and status
bool IsRunOnStartupEnabled();
void ToggleRunOnStartup();

// Theme management
bool IsLauncherDarkMode();
void ToggleLauncherTheme();
