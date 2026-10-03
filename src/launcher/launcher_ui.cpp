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

HFONT g_hFontChevron = nullptr;
HFONT g_hFontInput = nullptr;
HFONT g_hFontPrimary = nullptr;
HFONT g_hFontBadge = nullptr;
HFONT g_hFontHint = nullptr;
HFONT g_hFontKeycap = nullptr;

bool g_isDarkMode = true;
bool g_isExpanded = false;

const int WIN_WIDTH = 680;
const int HEIGHT_COLLAPSED = 58;
const int HEIGHT_EXPANDED = 148;

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
    std::wstring badgeText;
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
    return true; // Default dark
}

std::wstring GetEditText() {
    int len = GetWindowTextLengthW(g_hEdit);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(g_hEdit, buf.data(), len + 1);
    return buf.data();
}

void UpdateLauncherDimensions(bool expand) {
    if (g_isExpanded == expand && g_hMainWnd) return;
    g_isExpanded = expand;

    if (!g_hMainWnd) return;

    int newHeight = g_isExpanded ? HEIGHT_EXPANDED : HEIGHT_COLLAPSED;

    SetWindowPos(g_hMainWnd, nullptr, 0, 0, WIN_WIDTH, newHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Rounded window region
    HRGN hRgn = CreateRoundRectRgn(0, 0, WIN_WIDTH + 1, newHeight + 1, 20, 20);
    SetWindowRgn(g_hMainWnd, hRgn, TRUE);

    InvalidateRect(g_hMainWnd, nullptr, TRUE);
}

void EvaluateQuery(const std::wstring& rawQuery) {
    std::wstring query = rawQuery;
    size_t s = query.find_first_not_of(L" \t");
    if (s == std::wstring::npos) {
        g_action.type = ActionType::Empty;
        g_action.badgeText.clear();
        g_action.primaryText.clear();
        g_action.secondaryText.clear();
        g_action.payload.clear();
        UpdateLauncherDimensions(false); // Collapsed: ONLY Search Bar!
        return;
    }
    query = query.substr(s);
    UpdateLauncherDimensions(true); // Expanded: Show results!

    // 1. Unit conversion
    if (LooksLikeUnitConversion(query)) {
        UnitConvResult uRes = ConvertUnits(query);
        if (uRes.success) {
            g_action.type = ActionType::UnitConv;
            g_action.badgeText = L"Unit";
            g_action.primaryText = uRes.formatted;
            g_action.secondaryText = L"Copy to clipboard";
            g_action.payload = uRes.formatted;
            return;
        }
    }

    // 2. Math Expression
    if (LooksLikeMath(query)) {
        MathResult mRes = EvaluateMath(query);
        if (mRes.success) {
            g_action.type = ActionType::Math;
            g_action.badgeText = L"Math";
            g_action.primaryText = L"= " + mRes.formatted;
            g_action.secondaryText = L"Copy to clipboard";
            g_action.payload = mRes.formatted;
            return;
        }
    }

    // 3. System Commands (lock, sleep, restart, trash)
    std::wstring sysDesc;
    if (SysControl::Match(query, sysDesc)) {
        g_action.type = ActionType::SysCommand;
        g_action.badgeText = L"System";
        g_action.primaryText = sysDesc;
        g_action.secondaryText = L"Execute system command";
        g_action.payload = query;
        return;
    }

    // 4. Built-in SuperC tools (wifi, port, pass, hex, ts)
    if (query == L"wifi" || query.rfind(L"wifi ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.badgeText = L"Wi-Fi";
        g_action.primaryText = L"Reveal Saved Wi-Fi Password";
        g_action.secondaryText = L"View & Copy Password";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"port ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.badgeText = L"Port";
        g_action.primaryText = L"Inspect Port: " + query.substr(5);
        g_action.secondaryText = L"Find process using port";
        g_action.payload = query;
        return;
    }
    if (query == L"pass" || query.rfind(L"pass ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.badgeText = L"PassGen";
        g_action.primaryText = L"Generate Secure Password";
        g_action.secondaryText = L"Copy to clipboard";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"hex ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.badgeText = L"Hex/Dec";
        g_action.primaryText = L"Convert: " + query.substr(4);
        g_action.secondaryText = L"Hex, Dec & Binary";
        g_action.payload = query;
        return;
    }
    if (query == L"ts" || query.rfind(L"ts ", 0) == 0) {
        g_action.type = ActionType::BuiltinUtility;
        g_action.badgeText = L"Timestamp";
        g_action.primaryText = L"Convert Unix Epoch Timestamp";
        g_action.secondaryText = L"Local date and time";
        g_action.payload = query;
        return;
    }

    // 5. Open Window Switcher
    auto openWins = WindowWalker::SearchOpenWindows(query);
    if (!openWins.empty()) {
        g_action.type = ActionType::OpenWindow;
        g_action.badgeText = L"Window";
        g_action.primaryText = openWins[0].title;
        g_action.secondaryText = L"Switch to " + openWins[0].processName;
        g_action.targetWindow = openWins[0].hwnd;
        return;
    }

    // 6. Installed Applications
    auto apps = AppIndexer::Instance().Search(query, 1);
    if (!apps.empty()) {
        g_action.type = ActionType::App;
        g_action.badgeText = L"App";
        g_action.primaryText = apps[0].name;
        g_action.secondaryText = L"Launch application";
        g_action.targetApp = apps[0];
        return;
    }

    // 7. Web Search prefixes (g, yt, gh)
    if (query.rfind(L"g ", 0) == 0) {
        g_action.type = ActionType::WebSearch;
        g_action.badgeText = L"Google";
        g_action.primaryText = L"Search: \"" + query.substr(2) + L"\"";
        g_action.secondaryText = L"Open search in browser";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"yt ", 0) == 0) {
        g_action.type = ActionType::WebSearch;
        g_action.badgeText = L"YouTube";
        g_action.primaryText = L"Search: \"" + query.substr(3) + L"\"";
        g_action.secondaryText = L"Open YouTube in browser";
        g_action.payload = query;
        return;
    }
    if (query.rfind(L"gh ", 0) == 0) {
        g_action.type = ActionType::WebSearch;
        g_action.badgeText = L"GitHub";
        g_action.primaryText = query.substr(3);
        g_action.secondaryText = L"Open repo / search in browser";
        g_action.payload = query;
        return;
    }

    // 8. Fallback: Shell command
    g_action.type = ActionType::ShellCommand;
    g_action.badgeText = L"Command";
    g_action.primaryText = query;
    g_action.secondaryText = L"Run in terminal";
    g_action.payload = query;
}

void ExecuteCurrentAction(bool isElevated) {
    switch (g_action.type) {
        case ActionType::Math:
        case ActionType::UnitConv: {
            if (!g_action.payload.empty()) {
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
            g_isDarkMode = DetectWindowsDarkMode();

            // Native Windows 11 Rounded Corners
            DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

            // Native Immersive Dark Mode for DWM borders
            BOOL dm = g_isDarkMode ? TRUE : FALSE;
            DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dm, sizeof(dm));

            // Native Windows 11 Acrylic Backdrop Effect (3 = DWMSBT_TRANSIENTWINDOW)
            DWORD backdrop = 3;
            DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &backdrop, sizeof(backdrop));

            // Extend frame for translucent backdrop blur
            MARGINS margins = { -1, -1, -1, -1 };
            DwmExtendFrameIntoClientArea(hwnd, &margins);

            // Typography
            g_hFontChevron = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontInput = CreateFontW(-19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontPrimary = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontBadge = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontHint = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontKeycap = CreateFontW(-10, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Seamless borderless Edit control inside the top search pill
            g_hEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                46, 14, WIN_WIDTH - 64, 28,
                hwnd, (HMENU)101, GetModuleHandle(nullptr), nullptr
            );
            SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFontInput, TRUE);

            // Native cue banner placeholder
            SendMessageW(g_hEdit, 0x1501 /* EM_SETCUEBANNER */, TRUE, (LPARAM)L"Type a command, calculation (e.g. 5+7), or search...");

            g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            EvaluateQuery(L"");
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Double-buffering prevents flicker

        case WM_COMMAND: {
            if (LOWORD(wParam) == 101 && HIWORD(wParam) == EN_CHANGE) {
                std::wstring text = GetEditText();
                EvaluateQuery(text);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            if (g_isDarkMode) {
                SetTextColor(hdc, RGB(250, 250, 250));
                SetBkColor(hdc, RGB(34, 37, 45));
                static HBRUSH hBrDark = CreateSolidBrush(RGB(34, 37, 45));
                return (LRESULT)hBrDark;
            } else {
                SetTextColor(hdc, RGB(20, 20, 24));
                SetBkColor(hdc, RGB(244, 245, 248));
                static HBRUSH hBrLight = CreateSolidBrush(RGB(244, 245, 248));
                return (LRESULT)hBrLight;
            }
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdcScreen = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;

            // Off-screen double buffer
            HDC hdc = CreateCompatibleDC(hdcScreen);
            HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hdc, hBmp);

            // Translucent glass palette
            COLORREF bgCol = g_isDarkMode ? RGB(22, 24, 30) : RGB(255, 255, 255);
            COLORREF borderCol = g_isDarkMode ? RGB(58, 62, 74) : RGB(218, 222, 230);
            COLORREF chevronCol = RGB(0, 200, 255); // Electric cyan

            // Top Search Bar Capsule
            COLORREF searchBarBg = g_isDarkMode ? RGB(34, 37, 45) : RGB(244, 245, 248);
            COLORREF searchBarBorder = g_isDarkMode ? RGB(54, 58, 70) : RGB(222, 225, 232);

            // Bottom Result Card
            COLORREF resultCardBg = g_isDarkMode ? RGB(28, 30, 38) : RGB(249, 250, 253);
            COLORREF resultCardBorder = g_isDarkMode ? RGB(48, 52, 64) : RGB(226, 228, 236);

            COLORREF primaryTextCol = g_isDarkMode ? RGB(255, 255, 255) : RGB(18, 18, 22);
            COLORREF secondaryTextCol = g_isDarkMode ? RGB(145, 148, 160) : RGB(115, 118, 128);

            // Badge styling
            COLORREF badgeBg = g_isDarkMode ? RGB(36, 48, 72) : RGB(230, 242, 255);
            COLORREF badgeBorder = g_isDarkMode ? RGB(50, 85, 140) : RGB(175, 210, 255);
            COLORREF badgeText = g_isDarkMode ? RGB(75, 190, 255) : RGB(0, 110, 220);

            // Keycap styling
            COLORREF keyBg = g_isDarkMode ? RGB(40, 42, 50) : RGB(238, 240, 244);
            COLORREF keyBorder = g_isDarkMode ? RGB(62, 66, 78) : RGB(212, 215, 222);
            COLORREF keyText = g_isDarkMode ? RGB(210, 212, 222) : RGB(65, 68, 78);

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

            // 4. Bottom Result Card (ONLY drawn when expanded!)
            if (g_isExpanded) {
                // Clear visual separation: 8px gap between input bar and result card!
                RECT rcResultCard = { 6, 58, width - 6, height - 6 };
                HBRUSH hBrCard = CreateSolidBrush(resultCardBg);
                HPEN hPenCard = CreatePen(PS_SOLID, 1, resultCardBorder);
                SelectObject(hdc, hPenCard);
                SelectObject(hdc, hBrCard);
                RoundRect(hdc, rcResultCard.left, rcResultCard.top, rcResultCard.right, rcResultCard.bottom, 14, 14);
                DeleteObject(hBrCard);
                DeleteObject(hPenCard);

                // Category Badge Pill
                int badgeX = 18;
                int badgeY = 70;
                if (!g_action.badgeText.empty()) {
                    SelectObject(hdc, g_hFontBadge);
                    SetTextColor(hdc, badgeText);
                    SIZE sizeBadge;
                    GetTextExtentPoint32W(hdc, g_action.badgeText.c_str(), static_cast<int>(g_action.badgeText.length()), &sizeBadge);

                    int padX = 9;
                    int badgeW = sizeBadge.cx + (padX * 2);
                    int badgeH = 20;

                    RECT rcBadge = { badgeX, badgeY, badgeX + badgeW, badgeY + badgeH };
                    HBRUSH hBrdg = CreateSolidBrush(badgeBg);
                    HPEN hPndg = CreatePen(PS_SOLID, 1, badgeBorder);
                    SelectObject(hdc, hPndg);
                    SelectObject(hdc, hBrdg);
                    RoundRect(hdc, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom, 6, 6);
                    DeleteObject(hBrdg);
                    DeleteObject(hPndg);

                    DrawTextW(hdc, g_action.badgeText.c_str(), -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    badgeX += badgeW + 10;
                }

                // Primary Result Text
                SelectObject(hdc, g_hFontPrimary);
                SetTextColor(hdc, primaryTextCol);
                RECT rcPrimary = { badgeX, 66, width - 20, 94 };
                DrawTextW(hdc, g_action.primaryText.c_str(), -1, &rcPrimary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                // Footer: Secondary subtitle on left
                SelectObject(hdc, g_hFontHint);
                SetTextColor(hdc, secondaryTextCol);
                RECT rcSecondary = { 18, 108, 320, 130 };
                DrawTextW(hdc, g_action.secondaryText.c_str(), -1, &rcSecondary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                // Footer: Interactive Keycaps on right
                int rightX = width - 18;
                DrawKeyBadge(hdc, rightX, 108, L"Esc", L"Close", keyBg, keyBorder, keyText, secondaryTextCol);
                DrawKeyBadge(hdc, rightX, 108, L"Ctrl+\x21B5", L"Admin", keyBg, keyBorder, keyText, secondaryTextCol);
                DrawKeyBadge(hdc, rightX, 108, L"\x21B5 Enter", L"Run", keyBg, keyBorder, keyText, secondaryTextCol);
            }

            // Blit off-screen buffer to display
            BitBlt(hdcScreen, 0, 0, width, height, hdc, 0, 0, SRCCOPY);

            SelectObject(hdc, hOldBmp);
            DeleteObject(hBmp);
            DeleteDC(hdc);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                HideLauncher();
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

    // Translucent glass opacity (240 / 255 = ~94% opacity with blurred background)
    SetLayeredWindowAttributes(g_hMainWnd, 0, 242, LWA_ALPHA);

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

    // Always start collapsed!
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
