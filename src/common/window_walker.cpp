#include "window_walker.h"
#include <dwmapi.h>
#include <psapi.h>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")

namespace {

std::wstring ToLower(const std::wstring& str) {
    std::wstring res = str;
    for (auto& c : res) c = towlower(c);
    return res;
}

struct EnumContext {
    std::wstring query;
    std::vector<WindowEntry> results;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    // Check title length
    int len = GetWindowTextLengthW(hwnd);
    if (len == 0) return TRUE;

    // Filter tool windows
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    // Filter cloaked windows (hidden UWP/Store apps)
    int cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0) {
        return TRUE;
    }

    std::vector<wchar_t> titleBuf(len + 1);
    GetWindowTextW(hwnd, titleBuf.data(), len + 1);
    std::wstring title = titleBuf.data();

    // Ignore known system background windows
    if (title == L"Program Manager" || title == L"Windows Shell Experience Host") return TRUE;

    EnumContext* ctx = reinterpret_cast<EnumContext*>(lParam);
    std::wstring lowerTitle = ToLower(title);
    std::wstring lowerQuery = ToLower(ctx->query);

    size_t pos = lowerTitle.find(lowerQuery);
    if (pos != std::wstring::npos) {
        WindowEntry entry;
        entry.hwnd = hwnd;
        entry.title = title;
        entry.score = 200 - static_cast<int>(pos * 5);

        // Get process name
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != 0) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                wchar_t procPath[MAX_PATH] = { 0 };
                DWORD procLen = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, procPath, &procLen)) {
                    std::wstring full(procPath);
                    size_t slash = full.find_last_of(L"\\/");
                    if (slash != std::wstring::npos) {
                        entry.processName = full.substr(slash + 1);
                    }
                }
                CloseHandle(hProc);
            }
        }

        ctx->results.push_back(entry);
    }

    return TRUE;
}

} // namespace

std::vector<WindowEntry> WindowWalker::SearchOpenWindows(const std::wstring& query) {
    if (query.empty()) return {};

    EnumContext ctx;
    ctx.query = query;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    std::sort(ctx.results.begin(), ctx.results.end(), [](const WindowEntry& a, const WindowEntry& b) {
        return a.score > b.score;
    });

    if (ctx.results.size() > 5) {
        ctx.results.resize(5);
    }

    return ctx.results;
}

bool WindowWalker::SwitchTo(HWND hwnd) {
    if (!IsWindow(hwnd)) return false;

    HWND hFore = GetForegroundWindow();
    DWORD foreThread = GetWindowThreadProcessId(hFore, nullptr);
    DWORD appThread = GetCurrentThreadId();

    if (foreThread != appThread) {
        AttachThreadInput(foreThread, appThread, TRUE);
        BringWindowToTop(hwnd);
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        AttachThreadInput(foreThread, appThread, FALSE);
    } else {
        BringWindowToTop(hwnd);
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    }

    return true;
}
