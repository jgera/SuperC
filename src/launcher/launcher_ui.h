#pragma once
#include <windows.h>
#include <string>

// Context Menu Command IDs
enum ContextMenuCmds {
    IDM_EDIT_UNDO = 2010,
    IDM_EDIT_CUT = 2011,
    IDM_EDIT_COPY = 2012,
    IDM_EDIT_PASTE = 2013,
    IDM_EDIT_SELECTALL = 2014,
    IDM_TOGGLE_THEME = 2001,
    IDM_RUN_ADMIN = 2002,
    IDM_COPY_RESULT = 2003,
    IDM_AUTOSTART = 2004,
    IDM_HELP = 2005,
    IDM_HIDE = 2006,
    IDM_EXIT = 2007,
    IDM_TOGGLE_APPS_ONLY = 2008,
    IDM_RESET_POS = 2009
};

// Resets launcher position back to center
void ResetLauncherPosition();

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
