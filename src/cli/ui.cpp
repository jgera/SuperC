#include "ui.h"
#include <commctrl.h>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

namespace {

HFONT g_hFontTitle = nullptr;
HFONT g_hFontNormal = nullptr;
HFONT g_hFontResult = nullptr;
HFONT g_hFontStatus = nullptr;

const int IDC_EDIT_RESULT = 1001;
const int IDC_BTN_COPY = 1002;
const int IDC_BTN_SECONDARY = 1003;
const int IDC_LBL_STATUS = 1004;

// Global pointer to active dialog config for the window proc
DialogConfig* g_activeConfig = nullptr;
HWND g_hEdit = nullptr;
HWND g_hBtnCopy = nullptr;
HWND g_hBtnSecondary = nullptr;

LRESULT CALLBACK DialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            if (!g_activeConfig) return 0;

            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int width = rcClient.right - rcClient.left;
            int height = rcClient.bottom - rcClient.top;

            int margin = 16;
            int y = 14;

            // Prompt / Header Label
            HWND hLblPrompt = CreateWindowExW(
                0, L"STATIC", g_activeConfig->prompt.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                margin, y, width - (margin * 2), 22,
                hwnd, nullptr, GetModuleHandle(nullptr), nullptr
            );
            SendMessage(hLblPrompt, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            y += 26;

            // Result Edit Box
            DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP;
            int editHeight = 36;
            if (g_activeConfig->multiline) {
                editStyle |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
                editHeight = 160;
            } else {
                editStyle |= ES_AUTOHSCROLL;
            }

            g_hEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", g_activeConfig->resultText.c_str(),
                editStyle,
                margin, y, width - (margin * 2), editHeight,
                hwnd, (HMENU)(INT_PTR)IDC_EDIT_RESULT, GetModuleHandle(nullptr), nullptr
            );
            SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFontResult, TRUE);
            y += editHeight + 8;

            // Status label: "Copied to clipboard"
            if (g_activeConfig->autoCopied) {
                HWND hLblStatus = CreateWindowExW(
                    0, L"STATIC", L"v  Copied to clipboard",
                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                    margin, y, 200, 20,
                    hwnd, (HMENU)(INT_PTR)IDC_LBL_STATUS, GetModuleHandle(nullptr), nullptr
                );
                SendMessage(hLblStatus, WM_SETFONT, (WPARAM)g_hFontStatus, TRUE);
            }

            // Buttons row
            int btnWidth = 140;
            int btnHeight = 32;
            int btnY = height - margin - btnHeight;

            int btnX = width - margin - btnWidth;
            g_hBtnCopy = CreateWindowExW(
                0, L"BUTTON", L"Copy && Close",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                btnX, btnY, btnWidth, btnHeight,
                hwnd, (HMENU)(INT_PTR)IDC_BTN_COPY, GetModuleHandle(nullptr), nullptr
            );
            SendMessage(g_hBtnCopy, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            if (!g_activeConfig->secondaryButtonText.empty()) {
                int secWidth = 120;
                btnX -= (secWidth + 8);
                g_hBtnSecondary = CreateWindowExW(
                    0, L"BUTTON", g_activeConfig->secondaryButtonText.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    btnX, btnY, secWidth, btnHeight,
                    hwnd, (HMENU)(INT_PTR)IDC_BTN_SECONDARY, GetModuleHandle(nullptr), nullptr
                );
                SendMessage(g_hBtnSecondary, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            }

            // Set text selected in edit control and give focus
            SetFocus(g_hEdit);
            SendMessage(g_hEdit, EM_SETSEL, 0, -1);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hStatic = (HWND)lParam;
            if (GetDlgCtrlID(hStatic) == IDC_LBL_STATUS) {
                SetTextColor(hdcStatic, RGB(34, 139, 34)); // Forest green
            } else {
                SetTextColor(hdcStatic, RGB(40, 40, 40));
            }
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_BTN_COPY || wmId == IDOK) {
                // Copy current edit text to clipboard
                int len = GetWindowTextLengthW(g_hEdit);
                if (len >= 0) {
                    std::vector<wchar_t> buf(len + 1);
                    GetWindowTextW(g_hEdit, buf.data(), len + 1);
                    CopyToClipboard(buf.data());
                }
                PostQuitMessage(0);
                return 0;
            } else if (wmId == IDC_BTN_SECONDARY) {
                if (g_activeConfig && g_activeConfig->onSecondaryAction) {
                    g_activeConfig->onSecondaryAction();
                }
                PostQuitMessage(0);
                return 0;
            } else if (wmId == IDCANCEL) {
                PostQuitMessage(0);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

bool CopyToClipboard(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    size_t bytes = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    void* pMem = GlobalLock(hMem);
    if (pMem) {
        memcpy(pMem, text.c_str(), bytes);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
    return true;
}

std::wstring GetClipboardText() {
    if (!OpenClipboard(nullptr)) return L"";
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return L"";
    }
    wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
    std::wstring result;
    if (pText) {
        result = pText;
        GlobalUnlock(hData);
    }
    CloseClipboard();
    return result;
}

void ShowResultDialog(const DialogConfig& config) {
    // Auto-copy immediately if configured
    if (config.autoCopied && !config.resultText.empty()) {
        CopyToClipboard(config.resultText);
    }

    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Register class once
    static bool registered = false;
    const wchar_t* CLASS_NAME = L"C_App_ResultDialog";
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = DialogWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        RegisterClassExW(&wc);
        registered = true;
    }

    // Prepare fonts
    if (!g_hFontNormal) {
        g_hFontNormal = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontResult = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontStatus = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    // Window dimensions
    int winWidth = config.multiline ? 560 : 480;
    int winHeight = config.multiline ? 300 : 180;

    // Center on current screen
    POINT ptCursor;
    GetCursorPos(&ptCursor);
    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMon, &mi);

    int posX = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - winWidth) / 2;
    int posY = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - winHeight) / 2;

    DialogConfig localConfig = config;
    g_activeConfig = &localConfig;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        config.title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX, posY, winWidth, winHeight,
        nullptr, nullptr, hInst, nullptr
    );

    if (!hwnd) return;

    SetForegroundWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                break;
            }
            if (msg.wParam == VK_RETURN && !config.multiline) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_COPY, BN_CLICKED), 0);
                break;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_activeConfig = nullptr;
}

void ShowHelpDialog() {
    DialogConfig cfg;
    cfg.title = L"SuperC - Quick Command Reference";
    cfg.prompt = L"Commands available from Windows Run (Win + R):";
    cfg.multiline = true;
    cfg.autoCopied = false;
    cfg.resultText = 
        L"CALCULATION & MATH:\r\n"
        L"  c 5+7              -> Evaluates expression and copies result\r\n"
        L"  c (12*4)/2.5       -> Supports +, -, *, /, ^, %, parentheses\r\n"
        L"  c sqrt(144)        -> Functions: sqrt, abs, round, sin, cos, etc.\r\n"
        L"  c hex 255          -> Shows 0xFF, decimal, and binary\r\n"
        L"  c ts 1727940000    -> Converts Unix timestamp to local date/time\r\n\r\n"
        L"SYSTEM & SHELL:\r\n"
        L"  c ipconfig         -> Runs command and keeps terminal open\r\n"
        L"  c # sfc /scannow   -> Runs command as Administrator (elevated)\r\n"
        L"  c sudo <cmd>       -> Same as # (elevated command)\r\n"
        L"  c ps <script>      -> Runs in PowerShell\r\n"
        L"  c wt <command>     -> Runs in Windows Terminal\r\n"
        L"  c                  -> Opens a fresh Command Prompt\r\n\r\n"
        L"DEVELOPER & NETWORKING:\r\n"
        L"  c port 3000        -> Shows PID using port & provides Kill button\r\n"
        L"  c kill node        -> Force terminates matching processes\r\n"
        L"  c pass 16          -> Generates random secure password\r\n"
        L"  c wifi             -> Shows current Wi-Fi & saved password\r\n"
        L"  c ip / c myip      -> Shows active local IP addresses\r\n"
        L"  c p <host>         -> Quick ping\r\n\r\n"
        L"WEB & SEARCH:\r\n"
        L"  c g <query>        -> Google search\r\n"
        L"  c yt <query>       -> YouTube search\r\n"
        L"  c gh <repo>        -> GitHub repo or search\r\n"
        L"  c localhost:3000   -> Opens URL directly in default browser\r\n\r\n"
        L"CLIPBOARD:\r\n"
        L"  c lower / c upper  -> Changes clipboard text case\r\n"
        L"  c trim             -> Trims whitespace from clipboard\r\n"
        L"  c count            -> Character, word, and line count\r\n\r\n"
        L"FOLDERS:\r\n"
        L"  c dl / dt / doc    -> Downloads / Desktop / Documents\r\n"
        L"  c temp / appdata   -> %TEMP% / %APPDATA%\r\n"
        L"  c hosts            -> Edit hosts file in Notepad (Admin)\r\n"
        L"  c env              -> System Environment Variables\r\n\r\n"
        L"SETUP:\r\n"
        L"  c install          -> Registers 'c' in Windows Run (App Paths)\r\n"
        L"  c uninstall        -> Unregisters 'c' from Windows Run";

    ShowResultDialog(cfg);
}
