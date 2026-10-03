#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
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

namespace {

HWND g_hMainWnd = nullptr;
HWND g_hEdit = nullptr;
WNDPROC g_oldEditProc = nullptr;

HFONT g_hFontInput = nullptr;
HFONT g_hFontPrimary = nullptr;
HFONT g_hFontSecondary = nullptr;

bool g_isDarkMode = true;

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

struct CurrentAction {
    ActionType type = ActionType::Empty;
    std::wstring primaryText;
    std::wstring secondaryText;
    std::wstring payload;
    HWND targetWindow = nullptr;
    AppEntry targetApp;
    bool isElevated = false;
} g_action;

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
    return true; // Default to dark mode
}

std::wstring GetEditText() {
    int len = GetWindowTextLengthW(g_hEdit);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(g_hEdit, buf.data(), len + 1);
    return buf.data();
}

void EvaluateQuery(const std::wstring& rawQuery) {
    std::wstring query = rawQuery;
    size_t s = query.find_first_not_of(L" \t");
    if (s == std::wstring::npos) {
        g_action.type = ActionType::Empty;
        g_action.primaryText = L"Type a command, calculation (e.g. 5+7), or search...";
        g_action.secondaryText = L"Ctrl + Space to toggle  •  Esc to close";
        g_action.payload.clear();
        return;
    }
    query = query.substr(s);

    // 1. Unit conversion
    if (LooksLikeUnitConversion(query)) {
        UnitConvResult uRes = ConvertUnits(query);
        if (uRes.success) {
            g_action.type = ActionType::UnitConv;
            g_action.primaryText = uRes.formatted;
            g_action.secondaryText = L"Unit Conversion  •  ↵ Enter to copy to clipboard";
            g_action.payload = uRes.formatted;
            return;
        }
    }

    // 2. Math Expression
    if (LooksLikeMath(query)) {
        MathResult mRes = EvaluateMath(query);
        if (mRes.success) {
            g_action.type = ActionType::Math;
            g_action.primaryText = L"= " + mRes.formatted;
            g_action.secondaryText = L"Calculation  •  ↵ Enter to copy to clipboard";
            g_action.payload = mRes.formatted;
            return;
        }
    }

    // 3. System Commands (lock, sleep, restart, trash)
    std::wstring sysDesc;
    if (SysControl::Match(query, sysDesc)) {
        g_action.type = ActionType::SysCommand;
        g_action.primaryText = L"System: " + sysDesc;
        g_action.secondaryText = L"System Action  •  ↵ Enter to execute";
        g_action.payload = query;
        return;
    }

    // 4. Built-in SuperC keywords (wifi, port, pass, hex, ts, etc.)
    if (query == L"wifi" || query.rfind(L"wifi ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.primaryText = L"Wi-Fi: Reveal Saved Password";
        g_action.secondaryText = L"SuperC Tool  •  ↵ Enter to view & copy";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"port ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.primaryText = L"Port Inspector: " + query;
        g_action.secondaryText = L"SuperC Tool  •  ↵ Enter to inspect & kill process";
        g_action.payload = query;
        return;
    }
    if (query == L"pass" || query.rfind(L"pass ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.primaryText = L"Password Generator: " + query;
        g_action.secondaryText = L"SuperC Tool  •  ↵ Enter to generate & copy";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"hex ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.primaryText = L"Hex / Decimal Converter: " + query;
        g_action.secondaryText = L"SuperC Tool  •  ↵ Enter to convert & copy";
        g_action.payload = query;
        return;
    }
    if (query == L"ts" || query.rfind(L"ts ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.primaryText = L"Unix Timestamp: " + query;
        g_action.secondaryText = L"SuperC Tool  •  ↵ Enter to view & copy";
        g_action.payload = query;
        return;
    }

    // 5. Open Window Switcher
    auto openWins = WindowWalker::SearchOpenWindows(query);
    if (!openWins.empty()) {
        g_action.type = ActionType::OpenWindow;
        g_action.primaryText = L"Switch to: " + openWins[0].title;
        g_action.secondaryText = L"Active Window (" + openWins[0].processName + L")  •  ↵ Enter to switch";
        g_action.targetWindow = openWins[0].hwnd;
        return;
    }

    // 6. Installed Applications
    auto apps = AppIndexer::Instance().Search(query, 1);
    if (!apps.empty()) {
        g_action.type = ActionType::App;
        g_action.primaryText = L"Launch: " + apps[0].name;
        g_action.secondaryText = L"Application  •  ↵ Enter to launch";
        g_action.targetApp = apps[0];
        return;
    }

    // 7. Web Search prefixes (g, yt, gh)
    if (query.rfind(L"g ", 0) == 0) {
        g_action.type = ActionType::WebSearch;
        g_action.primaryText = L"Search Google: \"" + query.substr(2) + L"\"";
        g_action.secondaryText = L"Web Search  •  ↵ Enter to search in browser";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"yt ", 0) == 0) {
        g_action.type = ActionType::WebSearch;
        g_action.primaryText = L"Search YouTube: \"" + query.substr(3) + L"\"";
        g_action.secondaryText = L"Web Search  •  ↵ Enter to search in browser";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"gh ", 0) == 0) {
        g_action.type = ActionType::WebSearch;
        g_action.primaryText = L"GitHub: \"" + query.substr(3) + L"\"";
        g_action.secondaryText = L"Web Search  •  ↵ Enter to open in browser";
        g_action.payload = query;
        return;
    }

    // 8. Fallback: Shell command
    g_action.type = ActionType::ShellCommand;
    g_action.primaryText = L"Run: " + query;
    g_action.secondaryText = L"Terminal Command  •  ↵ Enter to run  •  Ctrl+Enter for Admin";
    g_action.payload = query;
}

void ExecuteCurrentAction(bool isElevated) {
    switch (g_action.type) {
        case ActionType::Math:
        case ActionType::UnitConv: {
            if (!g_action.payload.empty()) {
                // Copy to clipboard
                if (OpenClipboard(g_hMainWnd)) {
                    EmptyClipboard();
                    size_t bytes = (g_action.payload.length() + 1) * sizeof(wchar_t);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                    if (hMem) {
                        void* pMem = GlobalLock(hMem);
                        if (pMem) {
                            memcpy(pMem, g_action.payload.c_str(), bytes);
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
            SysControl::Execute(g_action.payload);
            break;
        }

        case ActionType::OpenWindow: {
            if (g_action.targetWindow) {
                WindowWalker::SwitchTo(g_action.targetWindow);
            }
            break;
        }

        case ActionType::App: {
            AppIndexer::Launch(g_action.targetApp);
            break;
        }

        case ActionType::BuiltinUtility: {
            // Forward to SuperC CLI handler functions
            std::wstring q = g_action.payload;
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
            std::wstring q = g_action.payload;
            if (q.rfind(L"g ", 0) == 0) Handlers::HandleGoogle(q.substr(2));
            else if (q.rfind(L"yt ", 0) == 0) Handlers::HandleYouTube(q.substr(3));
            else if (q.rfind(L"gh ", 0) == 0) Handlers::HandleGitHub(q.substr(3));
            break;
        }

        case ActionType::ShellCommand: {
            if (!g_action.payload.empty()) {
                Handlers::LaunchShell(g_action.payload, isElevated, L"cmd");
            }
            break;
        }

        default:
            break;
    }

    HideLauncher();
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            bool isElevated = (GetKeyState(VK_CONTROL) < 0);
            ExecuteCurrentAction(isElevated);
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
    return CallWindowProcW(g_oldEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK LauncherWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Detect theme
            g_isDarkMode = DetectWindowsDarkMode();

            // Set DWM rounded corners (Windows 11)
            DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

            // Fonts
            g_hFontInput = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontPrimary = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontSecondary = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Create Edit control for search input
            g_hEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                48, 16, 600, 32,
                hwnd, (HMENU)101, GetModuleHandle(nullptr), nullptr
            );
            SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFontInput, TRUE);

            // Subclass Edit control to intercept Enter & Esc
            g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            // Initial query evaluation
            EvaluateQuery(L"");
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == 101 && HIWORD(wParam) == EN_CHANGE) {
                std::wstring text = GetEditText();
                EvaluateQuery(text);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            if (g_isDarkMode) {
                SetTextColor(hdc, RGB(245, 245, 245));
                SetBkColor(hdc, RGB(32, 32, 35));
                return (LRESULT)GetStockObject(BLACK_BRUSH);
            } else {
                SetTextColor(hdc, RGB(25, 25, 25));
                SetBkColor(hdc, RGB(255, 255, 255));
                return (LRESULT)GetStockObject(WHITE_BRUSH);
            }
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            COLORREF bgCol = g_isDarkMode ? RGB(32, 32, 35) : RGB(255, 255, 255);
            COLORREF borderCol = g_isDarkMode ? RGB(60, 60, 65) : RGB(220, 220, 225);
            COLORREF primaryTextCol = g_isDarkMode ? RGB(245, 245, 245) : RGB(20, 20, 20);
            COLORREF secondaryTextCol = g_isDarkMode ? RGB(160, 160, 165) : RGB(120, 120, 125);
            COLORREF accentCol = g_isDarkMode ? RGB(100, 180, 255) : RGB(0, 110, 220);

            // Fill background
            HBRUSH hBrBg = CreateSolidBrush(bgCol);
            FillRect(hdc, &rc, hBrBg);
            DeleteObject(hBrBg);

            // Draw border
            HPEN hPenBorder = CreatePen(PS_SOLID, 1, borderCol);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPenBorder);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rc.left, rc.top, rc.right - 1, rc.bottom - 1, 16, 16);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPenBorder);

            SetBkMode(hdc, TRANSPARENT);

            // Search Icon: Draw clean magnifying glass prompt
            SelectObject(hdc, g_hFontInput);
            SetTextColor(hdc, accentCol);
            RECT rcIcon = { 18, 16, 44, 48 };
            DrawTextW(hdc, L">", -1, &rcIcon, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Divider line
            HPEN hPenDivider = CreatePen(PS_SOLID, 1, borderCol);
            SelectObject(hdc, hPenDivider);
            MoveToEx(hdc, 16, 58, nullptr);
            LineTo(hdc, rc.right - 16, 58);
            DeleteObject(hPenDivider);

            // Live Preview: Primary Text
            SelectObject(hdc, g_hFontPrimary);
            SetTextColor(hdc, primaryTextCol);
            RECT rcPrimary = { 20, 66, rc.right - 20, 96 };
            DrawTextW(hdc, g_action.primaryText.c_str(), -1, &rcPrimary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Live Preview: Secondary / Hint Text
            SelectObject(hdc, g_hFontSecondary);
            SetTextColor(hdc, secondaryTextCol);
            RECT rcSecondary = { 20, 98, rc.right - 20, 124 };
            DrawTextW(hdc, g_action.secondaryText.c_str(), -1, &rcSecondary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                // Auto-dismiss when clicking outside
                HideLauncher();
            }
            return 0;
        }

        case WM_DESTROY: {
            if (g_hFontInput) DeleteObject(g_hFontInput);
            if (g_hFontPrimary) DeleteObject(g_hFontPrimary);
            if (g_hFontSecondary) DeleteObject(g_hFontSecondary);
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
    wc.style = CS_DROPSHADOW;
    RegisterClassExW(&wc);

    int winWidth = 680;
    int winHeight = 136;

    // Centered horizontally, top 25% of screen
    POINT ptCursor;
    GetCursorPos(&ptCursor);
    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMon, &mi);

    int posX = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - winWidth) / 2;
    int posY = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) / 4);

    g_hMainWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"SuperC Quick Launcher",
        WS_POPUP,
        posX, posY, winWidth, winHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    return g_hMainWnd;
}

void ShowLauncher() {
    if (!g_hMainWnd) return;

    // Reposition to monitor of current cursor
    POINT ptCursor;
    GetCursorPos(&ptCursor);
    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMon, &mi);

    int winWidth = 680;
    int winHeight = 136;
    int posX = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - winWidth) / 2;
    int posY = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) / 4);

    SetWindowPos(g_hMainWnd, HWND_TOPMOST, posX, posY, winWidth, winHeight, SWP_SHOWWINDOW);
    ShowWindow(g_hMainWnd, SW_SHOW);
    SetForegroundWindow(g_hMainWnd);

    // Clear and focus edit
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
