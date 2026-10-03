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

bool IsExcludedNonApp(const std::wstring& name, bool appsOnlyMode) {
    if (!appsOnlyMode) return false;

    std::wstring lower = ToLower(name);

    // 1. Common non-executable keywords (documentation, uninstallers, helpers)
    static const wchar_t* junkKeywords[] = {
        L"uninstall", L"remove", L"setup", L"installer",
        L"readme", L"read me", L"license", L"documentation",
        L"manual", L"help", L"changelog", L"release notes",
        L"quick start", L"user guide", L"credits", L"website",
        L"homepage", L"register", L"privacy policy", L"terms",
        L"configuration", L"diagnostics"
    };
    for (const auto* kw : junkKeywords) {
        if (lower.find(kw) != std::wstring::npos) {
            return true;
        }
    }

    // 2. Non-application file extensions in shortcut name
    static const wchar_t* badExtensions[] = {
        L".txt", L".log", L".md", L".rtf", L".doc", L".docx",
        L".pdf", L".chm", L".hlp", L".url", L".htm", L".html",
        L".ini", L".cfg", L".json", L".xml", L".csv", L".xls", L".xlsx"
    };
    for (const auto* ext : badExtensions) {
        size_t elen = wcslen(ext);
        if (lower.length() >= elen && lower.compare(lower.length() - elen, elen, ext) == 0) {
            return true;
        }
    }

    return false;
}

} // namespace

AppIndexer& AppIndexer::Instance() {
    static AppIndexer instance;
    return instance;
}

AppIndexer::AppIndexer() {
    LoadSettings();
    RefreshIndex();
}

void AppIndexer::LoadSettings() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SuperC", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        DWORD size = sizeof(val);
        if (RegQueryValueExW(hKey, L"AppsOnlySearch", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            appsOnly = (val != 0);
        }
        RegCloseKey(hKey);
    }
}

void AppIndexer::SaveSettings() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SuperC", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = appsOnly ? 1 : 0;
        RegSetValueExW(hKey, L"AppsOnlySearch", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hKey);
    }
}

bool AppIndexer::GetAppsOnly() const {
    return appsOnly;
}

void AppIndexer::SetAppsOnly(bool enable) {
    if (appsOnly == enable) return;
    appsOnly = enable;
    SaveSettings();
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
                // Filter out non-applications when appsOnly is true
                if (!IsExcludedNonApp(name, appsOnly)) {
                    // Check duplicate names to keep index concise
                    bool exists = false;
                    for (const auto& a : apps) {
                        if (_wcsicmp(a.name.c_str(), name.c_str()) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        apps.push_back({ name, fullPath, 0 });
                    }
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

bool AppIndexer::Launch(const AppEntry& app, bool asAdmin) {
    const wchar_t* verb = asAdmin ? L"runas" : L"open";
    HINSTANCE hRes = ShellExecuteW(nullptr, verb, app.path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(hRes) > 32;
}
