#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>

#include "launcher_ui.h"
#include "../common/math_eval.h"
#include "../common/unit_conv.h"
#include "../common/app_index.h"
#include "../common/window_walker.h"
#include "../common/sys_control.h"
#include "../common/handlers.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

HWND g_hMainWnd = nullptr;
HWND g_hEdit = nullptr;
WNDPROC g_oldEditProc = nullptr;

HFONT g_hFontChevron = nullptr;
HFONT g_hFontInput = nullptr;
HFONT g_hFontPrimary = nullptr;
HFONT g_hFontBadge = nullptr;
HFONT g_hFontHint = nullptr;
HFONT g_hFontKeycap = nullptr;

bool g_isDarkMode = true;
bool g_isExpanded = false;
bool g_isMenuOpen = false;

const int WIN_WIDTH = 680;
const int HEIGHT_COLLAPSED = 58;



// Action types
enum class ActionType {
    Empty,
    Math,
    UnitConv,
    SysCommand,
    OpenWindow,
    App,
    BuiltinUtility,
    WebSearch,
    ShellCommand
};

struct LauncherResult {
    ActionType type = ActionType::Empty;
    std::wstring badgeText;
    std::wstring primaryText;
    std::wstring secondaryText;
    std::wstring payload;
    HWND targetWindow = nullptr;
    AppEntry targetApp;
    bool isElevated = false;
};

std::vector<LauncherResult> g_results;
int g_selectedIndex = 0;

bool DetectWindowsDarkMode() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (val == 0);
        }
        RegCloseKey(hKey);
    }
    return true; // Default dark
}

void SaveThemePreference(bool isDark) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SuperC", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = isDark ? 1 : 0;
        RegSetValueExW(hKey, L"DarkMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hKey);
    }
}

bool LoadThemePreference(bool& isDark) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SuperC", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hKey, L"DarkMode", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            isDark = (val != 0);
            return true;
        }
        RegCloseKey(hKey);
    }
    return false;
}

std::wstring GetLauncherExePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

std::wstring GetStartupShortcutPath() {
    wchar_t startupPath[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath) == S_OK) {
        return std::wstring(startupPath) + L"\\SuperC-Launcher.lnk";
    }
    return L"";
}

std::wstring GetEditText() {
    int len = GetWindowTextLengthW(g_hEdit);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(g_hEdit, buf.data(), len + 1);
    return buf.data();
}

int GetExpandedHeight() {
    if (!g_isExpanded || g_results.empty()) {
        return HEIGHT_COLLAPSED; // 58px
    }
    int count = static_cast<int>(g_results.size());
    // 58px search pill + (count * 52px items) + 32px footer
    return 58 + (count * 52) + 32;
}

void UpdateLauncherDimensions(bool expand) {
    g_isExpanded = expand;
    if (!g_hMainWnd) return;

    int newHeight = expand ? GetExpandedHeight() : HEIGHT_COLLAPSED;

    SetWindowPos(g_hMainWnd, nullptr, 0, 0, WIN_WIDTH, newHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Clean rounded window region
    HRGN hRgn = CreateRoundRectRgn(0, 0, WIN_WIDTH + 1, newHeight + 1, 20, 20);
    SetWindowRgn(g_hMainWnd, hRgn, TRUE);

    InvalidateRect(g_hMainWnd, nullptr, TRUE);
}

void EvaluateQuery(const std::wstring& rawQuery) {
    g_results.clear();
    g_selectedIndex = 0;

    std::wstring query = rawQuery;
    size_t s = query.find_first_not_of(L" \t");
    if (s == std::wstring::npos) {
        UpdateLauncherDimensions(false); // Collapsed: ONLY Search Bar!
        return;
    }
    query = query.substr(s);

    // 1. Unit conversion
    if (LooksLikeUnitConversion(query)) {
        UnitConvResult uRes = ConvertUnits(query);
        if (uRes.success) {
            LauncherResult item;
            item.type = ActionType::UnitConv;
            item.badgeText = L"Unit";
            item.primaryText = uRes.formatted;
            item.secondaryText = L"Copy to clipboard";
            item.payload = uRes.formatted;
            g_results.push_back(item);
            UpdateLauncherDimensions(true);
            return;
        }
    }

    // 2. Math Expression
    if (LooksLikeMath(query)) {
        MathResult mRes = EvaluateMath(query);
        if (mRes.success) {
            LauncherResult item;
            item.type = ActionType::Math;
            item.badgeText = L"Math";
            item.primaryText = L"= " + mRes.formatted;
            item.secondaryText = L"Copy to clipboard";
            item.payload = mRes.formatted;
            g_results.push_back(item);
            UpdateLauncherDimensions(true);
            return;
        }
    }

    // 3. System Commands (lock, sleep, restart, trash)
    std::wstring sysDesc;
    if (SysControl::Match(query, sysDesc)) {
        LauncherResult item;
        item.type = ActionType::SysCommand;
        item.badgeText = L"System";
        item.primaryText = sysDesc;
        item.secondaryText = L"Execute system command";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }

    // 4. Built-in SuperC tools (wifi, port, pass, hex, ts)
    if (query == L"wifi" || query.rfind(L"wifi ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::BuiltinUtility;
        item.badgeText = L"Wi-Fi";
        item.primaryText = L"Reveal Saved Wi-Fi Password";
        item.secondaryText = L"View & Copy Password";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }
    if (query.rfind(L"port ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::BuiltinUtility;
        item.badgeText = L"Port";
        item.primaryText = L"Inspect Port: " + query.substr(5);
        item.secondaryText = L"Find process using port";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }
    if (query == L"pass" || query.rfind(L"pass ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::BuiltinUtility;
        item.badgeText = L"PassGen";
        item.primaryText = L"Generate Secure Password";
        item.secondaryText = L"Copy to clipboard";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }
    if (query.rfind(L"hex ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::BuiltinUtility;
        item.badgeText = L"Hex/Dec";
        item.primaryText = L"Convert: " + query.substr(4);
        item.secondaryText = L"Hex, Dec & Binary";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }
    if (query == L"ts" || query.rfind(L"ts ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::BuiltinUtility;
        item.badgeText = L"Timestamp";
        item.primaryText = L"Convert Unix Epoch Timestamp";
        item.secondaryText = L"Local date and time";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }

    // 5. Open Windows and Installed Applications (Up to 3 total matches!)
    auto openWins = WindowWalker::SearchOpenWindows(query);
    auto apps = AppIndexer::Instance().Search(query, 3);

    if (!openWins.empty() || !apps.empty()) {
        // First add matching open windows
        for (const auto& w : openWins) {
            if (g_results.size() >= 3) break;
            LauncherResult item;
            item.type = ActionType::OpenWindow;
            item.badgeText = L"Window";
            item.primaryText = w.title;
            item.secondaryText = L"Switch to " + w.processName;
            item.targetWindow = w.hwnd;
            g_results.push_back(item);
        }

        // Fill remaining slots with matching installed applications
        for (const auto& a : apps) {
            if (g_results.size() >= 3) break;
            LauncherResult item;
            item.type = ActionType::App;
            item.badgeText = L"App";
            item.primaryText = a.name;
            item.secondaryText = L"Launch application";
            item.targetApp = a;
            g_results.push_back(item);
        }

        UpdateLauncherDimensions(true);
        return;
    }

    // 7. Web Search prefixes (g, yt, gh)
    if (query.rfind(L"g ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::WebSearch;
        item.badgeText = L"Google";
        item.primaryText = L"Search: \"" + query.substr(2) + L"\"";
        item.secondaryText = L"Open search in browser";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }
    if (query.rfind(L"yt ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::WebSearch;
        item.badgeText = L"YouTube";
        item.primaryText = L"Search: \"" + query.substr(3) + L"\"";
        item.secondaryText = L"Open YouTube in browser";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }
    if (query.rfind(L"gh ", 0) == 0) {
        LauncherResult item;
        item.type = ActionType::WebSearch;
        item.badgeText = L"GitHub";
        item.primaryText = query.substr(3);
        item.secondaryText = L"Open repo / search in browser";
        item.payload = query;
        g_results.push_back(item);
        UpdateLauncherDimensions(true);
        return;
    }

    // 8. Fallback: Shell command
    LauncherResult item;
    item.type = ActionType::ShellCommand;
    item.badgeText = L"Command";
    item.primaryText = query;
    item.secondaryText = L"Run in terminal";
    item.payload = query;
    g_results.push_back(item);
    UpdateLauncherDimensions(true);
}

void ExecuteResult(int index, bool isElevated) {
    if (index < 0 || index >= static_cast<int>(g_results.size())) return;
    const auto& res = g_results[index];

    switch (res.type) {
        case ActionType::Math:
        case ActionType::UnitConv: {
            if (!res.payload.empty()) {
                if (OpenClipboard(g_hMainWnd)) {
                    EmptyClipboard();
                    size_t bytes = (res.payload.length() + 1) * sizeof(wchar_t);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                    if (hMem) {
                        void* pMem = GlobalLock(hMem);
                        if (pMem) {
                            memcpy(pMem, res.payload.c_str(), bytes);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        }
                    }
                    CloseClipboard();
                }
            }
            break;
        }

        case ActionType::SysCommand: {
            SysControl::Execute(res.payload);
            break;
        }

        case ActionType::OpenWindow: {
            if (res.targetWindow) {
                WindowWalker::SwitchTo(res.targetWindow);
            }
            break;
        }

        case ActionType::App: {
            AppIndexer::Launch(res.targetApp, isElevated);
            break;
        }

        case ActionType::BuiltinUtility: {
            std::wstring q = res.payload;
            if (q == L"wifi" || q.rfind(L"wifi ", 0) == 0) Handlers::HandleWifi(L"");
            else if (q.rfind(L"port ", 0) == 0) Handlers::HandlePort(q.substr(5));
            else if (q == L"pass") Handlers::HandlePassword(L"");
            else if (q.rfind(L"pass ", 0) == 0) Handlers::HandlePassword(q.substr(5));
            else if (q.rfind(L"hex ", 0) == 0) Handlers::HandleHex(q.substr(4));
            else if (q == L"ts") Handlers::HandleTimestamp(L"");
            else if (q.rfind(L"ts ", 0) == 0) Handlers::HandleTimestamp(q.substr(3));
            break;
        }

        case ActionType::WebSearch: {
            std::wstring q = res.payload;
            if (q.rfind(L"g ", 0) == 0) Handlers::HandleGoogle(q.substr(2));
            else if (q.rfind(L"yt ", 0) == 0) Handlers::HandleYouTube(q.substr(3));
            else if (q.rfind(L"gh ", 0) == 0) Handlers::HandleGitHub(q.substr(3));
            break;
        }

        case ActionType::ShellCommand: {
            if (!res.payload.empty()) {
                Handlers::LaunchShell(res.payload, isElevated, L"cmd");
            }
            break;
        }

        default:
            break;
    }

    HideLauncher();
}

void ShowContextMenu(HWND hwnd, int screenX, int screenY, bool fromEdit) {
    if (g_isMenuOpen) return;
    g_isMenuOpen = true;

    HMENU hMenu = CreatePopupMenu();

    if (fromEdit) {
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_CUT, L"Cu&t\tCtrl+X");
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_COPY, L"&Copy\tCtrl+C");
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_PASTE, L"&Paste\tCtrl+V");
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_SELECTALL, L"Select &All\tCtrl+A");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    std::wstring themeLabel = g_isDarkMode ? L"Switch to Light Theme" : L"Switch to Dark Theme";
    AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_THEME, themeLabel.c_str());

    bool appsOnly = AppIndexer::Instance().GetAppsOnly();
    UINT appsFlags = MF_STRING | (appsOnly ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(hMenu, appsFlags, IDM_TOGGLE_APPS_ONLY, L"Applications Only (Exclude .txt/docs)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    if (!g_results.empty() && g_selectedIndex < static_cast<int>(g_results.size())) {
        AppendMenuW(hMenu, MF_STRING, IDM_RUN_ADMIN, L"Run as Administrator\tCtrl+Enter");
        AppendMenuW(hMenu, MF_STRING, IDM_COPY_RESULT, L"Execute / Copy Result\tEnter");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    bool autoStart = IsRunOnStartupEnabled();
    UINT autoFlags = MF_STRING | (autoStart ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(hMenu, autoFlags, IDM_AUTOSTART, L"Start with Windows");

    AppendMenuW(hMenu, MF_STRING, IDM_HELP, L"SuperC Documentation (GitHub)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_HIDE, L"Hide Launcher\tEsc");
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit SuperC");

    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, screenX, screenY, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);

    g_isMenuOpen = false;

    if (cmd == 0) {
        // If the user clicked outside the launcher to another app, hide launcher
        HWND fg = GetForegroundWindow();
        if (fg != g_hMainWnd && fg != g_hEdit) {
            HideLauncher();
        }
        return;
    }

    switch (cmd) {
        case IDM_EDIT_UNDO:
            SendMessageW(g_hEdit, WM_UNDO, 0, 0);
            break;
        case IDM_EDIT_CUT:
            SendMessageW(g_hEdit, WM_CUT, 0, 0);
            break;
        case IDM_EDIT_COPY:
            SendMessageW(g_hEdit, WM_COPY, 0, 0);
            break;
        case IDM_EDIT_PASTE:
            SendMessageW(g_hEdit, WM_PASTE, 0, 0);
            break;
        case IDM_EDIT_SELECTALL:
            SendMessageW(g_hEdit, EM_SETSEL, 0, -1);
            break;
        case IDM_TOGGLE_THEME:
            ToggleLauncherTheme();
            break;
        case IDM_TOGGLE_APPS_ONLY: {
            bool cur = AppIndexer::Instance().GetAppsOnly();
            AppIndexer::Instance().SetAppsOnly(!cur);
            EvaluateQuery(GetEditText());
            InvalidateRect(g_hMainWnd, nullptr, TRUE);
            break;
        }
        case IDM_RUN_ADMIN:
            ExecuteResult(g_selectedIndex, true);
            break;
        case IDM_COPY_RESULT:
            ExecuteResult(g_selectedIndex, false);
            break;
        case IDM_AUTOSTART:
            ToggleRunOnStartup();
            break;
        case IDM_HELP:
            ShellExecuteW(nullptr, L"open", L"https://github.com/jgera/SuperC", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case IDM_HIDE:
            HideLauncher();
            break;
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        default:
            break;
    }
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_DOWN) {
            if (g_selectedIndex + 1 < static_cast<int>(g_results.size())) {
                g_selectedIndex++;
                InvalidateRect(g_hMainWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (wParam == VK_UP) {
            if (g_selectedIndex > 0) {
                g_selectedIndex--;
                InvalidateRect(g_hMainWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (wParam == VK_RETURN) {
            bool isElevated = (GetKeyState(VK_CONTROL) < 0);
            ExecuteResult(g_selectedIndex, isElevated);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (GetWindowTextLengthW(hwnd) > 0) {
                SetWindowTextW(hwnd, L"");
            } else {
                HideLauncher();
            }
            return 0;
        }
    }

    if (msg == WM_RBUTTONUP) {
        POINT pt;
        GetCursorPos(&pt);
        ShowContextMenu(g_hMainWnd, pt.x, pt.y, true);
        return 0;
    }

    if (msg == WM_CONTEXTMENU) {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (x == -1 && y == -1) {
            RECT rc;
            GetWindowRect(hwnd, &rc);
            x = rc.left + 20;
            y = rc.bottom;
        }
        ShowContextMenu(g_hMainWnd, x, y, true);
        return 0;
    }

    return CallWindowProcW(g_oldEditProc, hwnd, msg, wParam, lParam);
}

void DrawKeyBadge(HDC hdc, int& curRightX, int y, const std::wstring& key, const std::wstring& action, COLORREF keyBg, COLORREF keyBorder, COLORREF keyText, COLORREF labelText) {
    SelectObject(hdc, g_hFontHint);
    SetTextColor(hdc, labelText);
    SIZE sizeAction;
    GetTextExtentPoint32W(hdc, action.c_str(), static_cast<int>(action.length()), &sizeAction);

    SelectObject(hdc, g_hFontKeycap);
    SetTextColor(hdc, keyText);
    SIZE sizeKey;
    GetTextExtentPoint32W(hdc, key.c_str(), static_cast<int>(key.length()), &sizeKey);

    int keyPadX = 7;
    int keyWidth = sizeKey.cx + (keyPadX * 2);
    int keyHeight = 18;

    int totalWidth = sizeAction.cx + keyWidth + 5;
    int startX = curRightX - totalWidth;

    RECT rcKey = { startX, y, startX + keyWidth, y + keyHeight };
    HBRUSH hBr = CreateSolidBrush(keyBg);
    HPEN hPen = CreatePen(PS_SOLID, 1, keyBorder);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
    RoundRect(hdc, rcKey.left, rcKey.top, rcKey.right, rcKey.bottom, 5, 5);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBr);
    DeleteObject(hBr);
    DeleteObject(hPen);

    DrawTextW(hdc, key.c_str(), -1, &rcKey, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, g_hFontHint);
    SetTextColor(hdc, labelText);
    RECT rcAction = { startX + keyWidth + 5, y + 1, startX + totalWidth, y + keyHeight };
    DrawTextW(hdc, action.c_str(), -1, &rcAction, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    curRightX = startX - 12;
}

LRESULT CALLBACK LauncherWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            bool savedDark = false;
            if (LoadThemePreference(savedDark)) {
                g_isDarkMode = savedDark;
            } else {
                g_isDarkMode = DetectWindowsDarkMode();
            }

            // Windows 11 Native Rounded Corners
            DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

            // Windows 11 Immersive Dark Mode for borders
            BOOL dm = g_isDarkMode ? TRUE : FALSE;
            DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dm, sizeof(dm));

            // High-contrast, crystal-clear typography (ANTIALIASED_QUALITY eliminates color fringing)
            g_hFontChevron = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontInput = CreateFontW(-19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontPrimary = CreateFontW(-17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontBadge = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontHint = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontKeycap = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Seamless borderless Edit control inside the search pill
            g_hEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                46, 14, WIN_WIDTH - 64, 28,
                hwnd, (HMENU)101, GetModuleHandle(nullptr), nullptr
            );
            SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFontInput, TRUE);

            // Cue banner placeholder
            SendMessageW(g_hEdit, 0x1501 /* EM_SETCUEBANNER */, TRUE, (LPARAM)L"Type a command, calculation (e.g. 5+7), or search...");

            g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            EvaluateQuery(L"");
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Double buffering handles painting cleanly

        case WM_COMMAND: {
            if (LOWORD(wParam) == 101 && HIWORD(wParam) == EN_CHANGE) {
                std::wstring text = GetEditText();
                EvaluateQuery(text);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            if (hCtl == g_hEdit) {
                if (g_isDarkMode) {
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkColor(hdc, RGB(28, 32, 44));
                    static HBRUSH hBrDark = CreateSolidBrush(RGB(28, 32, 44));
                    return (LRESULT)hBrDark;
                } else {
                    SetTextColor(hdc, RGB(15, 23, 42)); // High-contrast midnight black
                    SetBkColor(hdc, RGB(255, 255, 255));
                    static HBRUSH hBrLight = CreateSolidBrush(RGB(255, 255, 255));
                    return (LRESULT)hBrLight;
                }
            }
            break;
        }

        case WM_LBUTTONUP: {
            int y = GET_Y_LPARAM(lParam);
            int x = GET_X_LPARAM(lParam);
            if (g_isExpanded && y >= 58 && x >= 6 && x <= WIN_WIDTH - 6) {
                int clickedIndex = (y - 58) / 52;
                if (clickedIndex >= 0 && clickedIndex < static_cast<int>(g_results.size())) {
                    g_selectedIndex = clickedIndex;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    bool isElevated = (GetKeyState(VK_CONTROL) < 0);
                    ExecuteResult(g_selectedIndex, isElevated);
                    return 0;
                }
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdcScreen = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;

            // Off-screen double buffer for zero flicker
            HDC hdc = CreateCompatibleDC(hdcScreen);
            HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hdc, hBmp);

            // Premium Color Palettes
            COLORREF bgCol = g_isDarkMode ? RGB(16, 18, 24) : RGB(238, 242, 246);
            COLORREF borderCol = g_isDarkMode ? RGB(45, 52, 68) : RGB(203, 213, 225);
            COLORREF chevronCol = g_isDarkMode ? RGB(0, 225, 255) : RGB(0, 130, 220);

            // Top Search Bar Pill
            COLORREF searchBarBg = g_isDarkMode ? RGB(28, 32, 44) : RGB(255, 255, 255);
            COLORREF searchBarBorder = g_isDarkMode ? RGB(55, 64, 85) : RGB(203, 213, 225);

            // Item card normal vs active selection
            COLORREF cardNormalBg = g_isDarkMode ? RGB(22, 25, 34) : RGB(248, 250, 252);
            COLORREF cardNormalBorder = g_isDarkMode ? RGB(40, 45, 58) : RGB(226, 232, 240);

            COLORREF cardSelectedBg = g_isDarkMode ? RGB(34, 40, 56) : RGB(255, 255, 255);
            COLORREF cardSelectedBorder = g_isDarkMode ? RGB(0, 180, 255) : RGB(2, 132, 199);
            COLORREF accentBarCol = g_isDarkMode ? RGB(0, 225, 255) : RGB(2, 132, 199);

            // Text colors: Bold and high contrast
            COLORREF primaryTextCol = g_isDarkMode ? RGB(255, 255, 255) : RGB(15, 23, 42);
            COLORREF secondaryTextCol = g_isDarkMode ? RGB(148, 163, 184) : RGB(71, 85, 105);

            // Badge styling
            COLORREF badgeBg = g_isDarkMode ? RGB(12, 45, 72) : RGB(224, 242, 254);
            COLORREF badgeBorder = g_isDarkMode ? RGB(2, 132, 199) : RGB(125, 211, 252);
            COLORREF badgeText = g_isDarkMode ? RGB(56, 189, 248) : RGB(3, 105, 161);

            // Keycap styling
            COLORREF keyBg = g_isDarkMode ? RGB(36, 42, 56) : RGB(241, 245, 249);
            COLORREF keyBorder = g_isDarkMode ? RGB(58, 66, 88) : RGB(203, 213, 225);
            COLORREF keyText = g_isDarkMode ? RGB(226, 232, 240) : RGB(30, 41, 59);

            // 1. Overall window background
            HBRUSH hBrBg = CreateSolidBrush(bgCol);
            FillRect(hdc, &rc, hBrBg);
            DeleteObject(hBrBg);

            // 2. Outer window rounded border
            HPEN hPenBorder = CreatePen(PS_SOLID, 1, borderCol);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPenBorder);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right - 1, rc.bottom - 1, 20, 20);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPenBorder);

            SetBkMode(hdc, TRANSPARENT);

            // 3. Top Input Bar Pill Container: [6, 6, width-6, 50]
            RECT rcSearchPill = { 6, 6, width - 6, 50 };
            HBRUSH hBrPill = CreateSolidBrush(searchBarBg);
            HPEN hPenPill = CreatePen(PS_SOLID, 1, searchBarBorder);
            SelectObject(hdc, hPenPill);
            SelectObject(hdc, hBrPill);
            RoundRect(hdc, rcSearchPill.left, rcSearchPill.top, rcSearchPill.right, rcSearchPill.bottom, 14, 14);
            DeleteObject(hBrPill);
            DeleteObject(hPenPill);

            // Electric Chevron prompt ›
            SelectObject(hdc, g_hFontChevron);
            SetTextColor(hdc, chevronCol);
            RECT rcChevron = { 16, 12, 40, 44 };
            DrawTextW(hdc, L"\x203A", -1, &rcChevron, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // 4. Result Items List (Up to 3 items displayed!)
            if (g_isExpanded && !g_results.empty()) {
                int count = static_cast<int>(g_results.size());
                for (int i = 0; i < count; ++i) {
                    const auto& item = g_results[i];
                    bool isSelected = (i == g_selectedIndex);

                    int itemY = 58 + (i * 52);
                    RECT rcItem = { 6, itemY, width - 6, itemY + 48 };

                    COLORREF cBg = isSelected ? cardSelectedBg : cardNormalBg;
                    COLORREF cBorder = isSelected ? cardSelectedBorder : cardNormalBorder;

                    HBRUSH hBrCard = CreateSolidBrush(cBg);
                    HPEN hPenCard = CreatePen(PS_SOLID, isSelected ? 2 : 1, cBorder);
                    SelectObject(hdc, hPenCard);
                    SelectObject(hdc, hBrCard);
                    RoundRect(hdc, rcItem.left, rcItem.top, rcItem.right, rcItem.bottom, 12, 12);
                    DeleteObject(hBrCard);
                    DeleteObject(hPenCard);

                    // Left selection indicator accent pill
                    if (isSelected) {
                        HBRUSH hBrAccent = CreateSolidBrush(accentBarCol);
                        RECT rcAccent = { rcItem.left + 3, rcItem.top + 10, rcItem.left + 7, rcItem.bottom - 10 };
                        FillRect(hdc, &rcAccent, hBrAccent);
                        DeleteObject(hBrAccent);
                    }

                    // Category Badge Pill
                    int badgeX = rcItem.left + 16;
                    int badgeY = rcItem.top + 14;
                    int badgeW = 0;

                    if (!item.badgeText.empty()) {
                        SelectObject(hdc, g_hFontBadge);
                        SetTextColor(hdc, badgeText);
                        SIZE sizeBadge;
                        GetTextExtentPoint32W(hdc, item.badgeText.c_str(), static_cast<int>(item.badgeText.length()), &sizeBadge);

                        int padX = 8;
                        badgeW = sizeBadge.cx + (padX * 2);
                        int badgeH = 20;

                        RECT rcBadge = { badgeX, badgeY, badgeX + badgeW, badgeY + badgeH };
                        HBRUSH hBrdg = CreateSolidBrush(badgeBg);
                        HPEN hPndg = CreatePen(PS_SOLID, 1, badgeBorder);
                        SelectObject(hdc, hPndg);
                        SelectObject(hdc, hBrdg);
                        RoundRect(hdc, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom, 6, 6);
                        DeleteObject(hBrdg);
                        DeleteObject(hPndg);

                        DrawTextW(hdc, item.badgeText.c_str(), -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    // Primary Result Text
                    SelectObject(hdc, g_hFontPrimary);
                    SetTextColor(hdc, primaryTextCol);
                    RECT rcPrimary = { badgeX + badgeW + 12, rcItem.top + 8, width - 180, rcItem.bottom - 8 };
                    DrawTextW(hdc, item.primaryText.c_str(), -1, &rcPrimary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    // Secondary Subtitle / Action hint on right
                    SelectObject(hdc, g_hFontHint);
                    SetTextColor(hdc, isSelected ? primaryTextCol : secondaryTextCol);
                    RECT rcSec = { width - 220, rcItem.top + 8, width - 18, rcItem.bottom - 8 };
                    DrawTextW(hdc, item.secondaryText.c_str(), -1, &rcSec, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }

                // 5. Footer Bar underneath results
                int footerY = 58 + (count * 52) + 6;

                // Navigation hint on left
                SelectObject(hdc, g_hFontHint);
                SetTextColor(hdc, secondaryTextCol);
                RECT rcNav = { 18, footerY, 260, footerY + 22 };
                DrawTextW(hdc, L"\x2191\x2193 Navigate  \x2022  \x21B5 Run  \x2022  Ctrl+\x21B5 Admin", -1, &rcNav, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // Interactive Keycaps on right
                int rightX = width - 18;
                DrawKeyBadge(hdc, rightX, footerY + 2, L"Esc", L"Close", keyBg, keyBorder, keyText, secondaryTextCol);
                DrawKeyBadge(hdc, rightX, footerY + 2, L"Ctrl+\x21B5", L"Admin", keyBg, keyBorder, keyText, secondaryTextCol);
                DrawKeyBadge(hdc, rightX, footerY + 2, L"\x21B5 Enter", L"Run", keyBg, keyBorder, keyText, secondaryTextCol);
            }

            // Blit off-screen buffer to display
            BitBlt(hdcScreen, 0, 0, width, height, hdc, 0, 0, SRCCOPY);

            SelectObject(hdc, hOldBmp);
            DeleteObject(hBmp);
            DeleteDC(hdc);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            ShowContextMenu(hwnd, pt.x, pt.y, false);
            return 0;
        }

        case WM_CONTEXTMENU: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (x == -1 && y == -1) {
                POINT pt;
                GetCursorPos(&pt);
                x = pt.x;
                y = pt.y;
            }
            ShowContextMenu(hwnd, x, y, false);
            return 0;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                if (!g_isMenuOpen) {
                    HideLauncher();
                }
            }
            return 0;
        }

        case WM_DESTROY: {
            if (g_hFontChevron) DeleteObject(g_hFontChevron);
            if (g_hFontInput) DeleteObject(g_hFontInput);
            if (g_hFontPrimary) DeleteObject(g_hFontPrimary);
            if (g_hFontBadge) DeleteObject(g_hFontBadge);
            if (g_hFontHint) DeleteObject(g_hFontHint);
            if (g_hFontKeycap) DeleteObject(g_hFontKeycap);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

HWND CreateLauncherWindow(HINSTANCE hInstance) {
    const wchar_t* CLASS_NAME = L"SuperC_QuickLauncher_WndClass";

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = LauncherWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.style = CS_DROPSHADOW;
    RegisterClassExW(&wc);

    int posX = (GetSystemMetrics(SM_CXSCREEN) - WIN_WIDTH) / 2;
    int posY = GetSystemMetrics(SM_CYSCREEN) / 4;

    g_hMainWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME,
        L"SuperC Quick Launcher",
        WS_POPUP,
        posX, posY, WIN_WIDTH, HEIGHT_COLLAPSED,
        nullptr, nullptr, hInstance, nullptr
    );

    // Subtle premium glass transparency (~94% opacity)
    SetLayeredWindowAttributes(g_hMainWnd, 0, 240, LWA_ALPHA);

    // Initial collapsed rounded region
    HRGN hRgn = CreateRoundRectRgn(0, 0, WIN_WIDTH + 1, HEIGHT_COLLAPSED + 1, 20, 20);
    SetWindowRgn(g_hMainWnd, hRgn, TRUE);

    return g_hMainWnd;
}

void ShowLauncher() {
    if (!g_hMainWnd) return;

    POINT ptCursor;
    GetCursorPos(&ptCursor);
    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMon, &mi);

    int posX = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - WIN_WIDTH) / 2;
    int posY = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) / 4);

    // Always start collapsed
    g_isExpanded = false;
    SetWindowPos(g_hMainWnd, HWND_TOPMOST, posX, posY, WIN_WIDTH, HEIGHT_COLLAPSED, SWP_SHOWWINDOW);

    HRGN hRgn = CreateRoundRectRgn(0, 0, WIN_WIDTH + 1, HEIGHT_COLLAPSED + 1, 20, 20);
    SetWindowRgn(g_hMainWnd, hRgn, TRUE);

    ShowWindow(g_hMainWnd, SW_SHOW);
    SetForegroundWindow(g_hMainWnd);

    SetWindowTextW(g_hEdit, L"");
    SetFocus(g_hEdit);

    EvaluateQuery(L"");
    InvalidateRect(g_hMainWnd, nullptr, TRUE);
}

void HideLauncher() {
    if (!g_hMainWnd) return;
    ShowWindow(g_hMainWnd, SW_HIDE);
}

void ToggleLauncher() {
    if (IsLauncherVisible()) {
        HideLauncher();
    } else {
        ShowLauncher();
    }
}

bool IsLauncherVisible() {
    return g_hMainWnd && IsWindowVisible(g_hMainWnd);
}

bool IsRunOnStartupEnabled() {
    std::wstring link = GetStartupShortcutPath();
    if (link.empty()) return false;
    DWORD attr = GetFileAttributesW(link.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

void ToggleRunOnStartup() {
    std::wstring link = GetStartupShortcutPath();
    if (link.empty()) return;

    if (IsRunOnStartupEnabled()) {
        DeleteFileW(link.c_str());
    } else {
        CoInitialize(nullptr);
        IShellLinkW* psl = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl)))) {
            std::wstring target = GetLauncherExePath();
            psl->SetPath(target.c_str());
            psl->SetDescription(L"SuperC Quick Launcher");

            IPersistFile* ppf = nullptr;
            if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf)))) {
                ppf->Save(link.c_str(), TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        CoUninitialize();
    }
}

bool IsLauncherDarkMode() {
    return g_isDarkMode;
}

void ToggleLauncherTheme() {
    g_isDarkMode = !g_isDarkMode;
    SaveThemePreference(g_isDarkMode);
    if (g_hMainWnd) {
        BOOL dm = g_isDarkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(g_hMainWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dm, sizeof(dm));
        InvalidateRect(g_hMainWnd, nullptr, TRUE);
        InvalidateRect(g_hEdit, nullptr, TRUE);
    }
}
