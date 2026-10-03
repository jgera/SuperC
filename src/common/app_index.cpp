#include "app_index.h"
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>

namespace {

std::wstring ToLower(const std::wstring& str) {
    std::wstring res = str;
    for (auto& c : res) c = towlower(c);
    return res;
}

int CalculateFuzzyScore(const std::wstring& target, const std::wstring& query) {
    std::wstring t = ToLower(target);
    std::wstring q = ToLower(query);

    if (q.empty()) return 0;
    if (t == q) return 1000; // Exact match

    // Exact prefix match
    if (t.rfind(q, 0) == 0) {
        return 500 - static_cast<int>(t.length() - q.length());
    }

    // Exact substring match
    size_t subPos = t.find(q);
    if (subPos != std::wstring::npos) {
        return 300 - static_cast<int>(subPos * 5);
    }

    // Subsequence match with word-start bonuses
    size_t tIdx = 0;
    size_t qIdx = 0;
    int score = 0;
    bool prevMatched = false;

    while (tIdx < t.length() && qIdx < q.length()) {
        if (t[tIdx] == q[qIdx]) {
            score += 10;
            // Word start bonus (after space, dash, or at start)
            if (tIdx == 0 || t[tIdx - 1] == L' ' || t[tIdx - 1] == L'-' || t[tIdx - 1] == L'_') {
                score += 30;
            }
            if (prevMatched) {
                score += 15; // Consecutive bonus
            }
            prevMatched = true;
            qIdx++;
        } else {
            prevMatched = false;
        }
        tIdx++;
    }

    if (qIdx == q.length()) {
        return score;
    }

    return 0; // Not a match
}

} // namespace

AppIndexer& AppIndexer::Instance() {
    static AppIndexer instance;
    return instance;
}

AppIndexer::AppIndexer() {
    RefreshIndex();
}

void AppIndexer::RefreshIndex() {
    apps.clear();

    // 1. Common system utilities
    static const struct { const wchar_t* name; const wchar_t* path; } builtins[] = {
        { L"Notepad", L"notepad.exe" },
        { L"Calculator", L"calc.exe" },
        { L"Command Prompt", L"cmd.exe" },
        { L"PowerShell", L"powershell.exe" },
        { L"Task Manager", L"taskmgr.exe" },
        { L"Registry Editor", L"regedit.exe" },
        { L"Control Panel", L"control.exe" },
        { L"File Explorer", L"explorer.exe" },
        { L"Settings", L"ms-settings:" }
    };
    for (const auto& b : builtins) {
        apps.push_back({ b.name, b.path, 0 });
    }

    // 2. ProgramData Start Menu
    wchar_t commonPrograms[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonPrograms) == S_OK) {
        ScanDirectory(commonPrograms);
    }

    // 3. User Start Menu
    wchar_t userPrograms[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userPrograms) == S_OK) {
        ScanDirectory(userPrograms);
    }

    initialized = true;
}

void AppIndexer::ScanDirectory(const std::wstring& dir) {
    std::wstring searchPattern = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        std::wstring fullPath = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanDirectory(fullPath);
        } else {
            std::wstring file = fd.cFileName;
            if (file.length() > 4 && file.substr(file.length() - 4) == L".lnk") {
                std::wstring name = file.substr(0, file.length() - 4);
                // Skip uninstallers
                std::wstring lowerName = ToLower(name);
                if (lowerName.find(L"uninstall") == std::wstring::npos) {
                    apps.push_back({ name, fullPath, 0 });
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

std::vector<AppEntry> AppIndexer::Search(const std::wstring& query, size_t maxResults) {
    if (query.empty()) return {};

    std::vector<AppEntry> matches;
    for (const auto& app : apps) {
        int score = CalculateFuzzyScore(app.name, query);
        if (score > 0) {
            AppEntry match = app;
            match.score = score;
            matches.push_back(match);
        }
    }

    std::sort(matches.begin(), matches.end(), [](const AppEntry& a, const AppEntry& b) {
        return a.score > b.score;
    });

    if (matches.size() > maxResults) {
        matches.resize(maxResults);
    }

    return matches;
}

bool AppIndexer::Launch(const AppEntry& app) {
    HINSTANCE hRes = ShellExecuteW(nullptr, L"open", app.path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(hRes) > 32;
}
