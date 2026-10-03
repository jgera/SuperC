#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

#include "launcher_ui.h"
#include "../common/app_index.h"

#pragma comment(lib, "shell32.lib")

namespace {

const UINT WM_TRAYICON = WM_USER + 101;
const UINT ID_TRAY_SHOW = 2001;
const UINT ID_TRAY_STARTUP = 2002;
const UINT ID_TRAY_EXIT = 2003;
const UINT ID_TRAY_THEME = 2004;
const UINT ID_TRAY_HELP = 2005;

NOTIFYICONDATAW g_nid = { sizeof(NOTIFYICONDATAW) };
HWND g_hMsgWnd = nullptr;
HWND g_hLauncher = nullptr;

void ShowTrayContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_SHOW, L"Open Launcher (Ctrl + Space)");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

    std::wstring themeLabel = IsLauncherDarkMode() ? L"Switch to Light Theme" : L"Switch to Dark Theme";
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_THEME, themeLabel.c_str());

    UINT startupFlags = MF_BYPOSITION | MF_STRING;
    if (IsRunOnStartupEnabled()) startupFlags |= MF_CHECKED;
    InsertMenuW(hMenu, 3, startupFlags, ID_TRAY_STARTUP, L"Start with Windows");

    InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING, ID_TRAY_HELP, L"Documentation (GitHub)");

    InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, 6, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit SuperC");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_HOTKEY: {
            if (wParam == 1) {
                ToggleLauncher();
            }
            return 0;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONUP) {
                ToggleLauncher();
            } else if (lParam == WM_RBUTTONUP) {
                ShowTrayContextMenu(hwnd);
            }
            return 0;
        }

        case WM_COMMAND: {
            UINT cmd = LOWORD(wParam);
            if (cmd == ID_TRAY_SHOW) {
                ShowLauncher();
            } else if (cmd == ID_TRAY_THEME) {
                ToggleLauncherTheme();
            } else if (cmd == ID_TRAY_STARTUP) {
                ToggleRunOnStartup();
            } else if (cmd == ID_TRAY_HELP) {
                ShellExecuteW(nullptr, L"open", L"https://github.com/jgera/SuperC", nullptr, nullptr, SW_SHOWNORMAL);
            } else if (cmd == ID_TRAY_EXIT) {
                PostQuitMessage(0);
            }
            return 0;
        }

        case WM_DESTROY: {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            UnregisterHotKey(hwnd, 1);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    // Single instance mutex
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\SuperC_QuickLauncher_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Warm up app indexer
    AppIndexer::Instance().RefreshIndex();

    // Create background message-only window for hotkeys and tray
    const wchar_t* MSG_CLASS = L"SuperC_MsgWndClass";
    WNDCLASSEXW wcMsg = { sizeof(WNDCLASSEXW) };
    wcMsg.lpfnWndProc = MsgWndProc;
    wcMsg.hInstance = hInstance;
    wcMsg.lpszClassName = MSG_CLASS;
    RegisterClassExW(&wcMsg);

    g_hMsgWnd = CreateWindowExW(0, MSG_CLASS, L"SuperC_MsgWnd", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);

    // Create the floating launcher window (starts hidden)
    g_hLauncher = CreateLauncherWindow(hInstance);

    // Register global hotkey: Ctrl + Space
    RegisterHotKey(g_hMsgWnd, 1, MOD_CONTROL | MOD_NOREPEAT, VK_SPACE);

    // System tray icon
    g_nid.hWnd = g_hMsgWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    HICON hAppIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    g_nid.hIcon = hAppIcon ? hAppIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"SuperC Quick Launcher (Ctrl + Space)");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
