#include "app_index.h"
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <shellapi.h>
#include <algorithm>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace {

std::wstring ToLower(const std::wstring& str) {
    std::wstring res = str;
    for (auto& c : res) c = towlower(c);
    return res;
}

bool EndsWith(const std::wstring& str, const std::wstring& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
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

bool IsExcludedNonApp(const std::wstring& name, const std::wstring& path, bool appsOnlyMode) {
    if (!appsOnlyMode) return false;

    std::wstring lowerName = ToLower(name);
    std::wstring lowerPath = ToLower(path);

    // 1. Non-application and document file extensions in path or name
    static const wchar_t* badExtensions[] = {
        L".txt", L".log", L".md", L".rtf", L".doc", L".docx",
        L".pdf", L".chm", L".hlp", L".url", L".htm", L".html",
        L".ini", L".cfg", L".json", L".xml", L".csv", L".xls", L".xlsx",
        L".png", L".jpg", L".jpeg", L".ico", L".bmp", L".gif",
        L".zip", L".tar", L".gz", L".7z", L".rar"
    };
    for (const auto* ext : badExtensions) {
        if (EndsWith(lowerName, ext) || EndsWith(lowerPath, ext)) {
            return true;
        }
    }

    // 2. Non-executable junk / documentation keywords in name
    static const wchar_t* junkKeywords[] = {
        L"uninstall", L"remove ", L"setup", L"installer", L"unins0",
        L"readme", L"read me", L"license", L"documentation",
        L"manual", L"help", L"changelog", L"release notes",
        L"what's new", L"whats new", L"copying", L"credits",
        L"privacy policy", L"terms of", L"web site", L"website",
        L"homepage", L"deactivate", L"diagnostics"
    };
    for (const auto* kw : junkKeywords) {
        if (lowerName.find(kw) != std::wstring::npos) {
            return true;
        }
    }

    // 3. Junk in target path (uninstallers, deactivators)
    if (lowerPath.find(L"uninstall") != std::wstring::npos ||
        lowerPath.find(L"unins00") != std::wstring::npos ||
        lowerPath.find(L"deactivate") != std::wstring::npos) {
        return true;
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

bool AppIndexer::AddApp(const std::wstring& name, const std::wstring& path) {
    if (name.empty() || path.empty()) return false;
    for (const auto& a : apps) {
        if (_wcsicmp(a.name.c_str(), name.c_str()) == 0) {
            return false;
        }
    }
    apps.push_back({ name, path, 0 });
    return true;
}

void AppIndexer::ScanAppsFolder() {
    IShellItem* pAppsFolder = nullptr;
    HRESULT hr = SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&pAppsFolder));
    if (FAILED(hr)) {
        hr = SHCreateItemFromParsingName(L"shell:AppsFolder", nullptr, IID_PPV_ARGS(&pAppsFolder));
    }

    if (SUCCEEDED(hr) && pAppsFolder) {
        IEnumShellItems* pEnum = nullptr;
        hr = pAppsFolder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&pEnum));
        if (SUCCEEDED(hr) && pEnum) {
            IShellItem* pItem = nullptr;
            ULONG fetched = 0;
            while (pEnum->Next(1, &pItem, &fetched) == S_OK && fetched == 1) {
                LPWSTR pszName = nullptr;
                pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &pszName);
                LPWSTR pszPath = nullptr;
                pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pszPath);

                if (pszName && pszPath) {
                    std::wstring name(pszName);
                    std::wstring path(pszPath);
                    if (!IsExcludedNonApp(name, path, appsOnly)) {
                        AddApp(name, path);
                    }
                }

                if (pszName) CoTaskMemFree(pszName);
                if (pszPath) CoTaskMemFree(pszPath);
                pItem->Release();
            }
            pEnum->Release();
        }
        pAppsFolder->Release();
    }
}

void AppIndexer::RefreshIndex() {
    apps.clear();
    CoInitialize(nullptr);

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
        { L"Settings", L"ms-settings:" },
        { L"Paint", L"mspaint.exe" },
        { L"Snipping Tool", L"snippingtool.exe" }
    };
    for (const auto& b : builtins) {
        AddApp(b.name, b.path);
    }

    // 2. Modern Shell & Store Apps (WhatsApp, Spotify, Terminal, etc.)
    ScanAppsFolder();

    // 3. ProgramData Start Menu shortcuts
    wchar_t commonPrograms[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonPrograms) == S_OK) {
        ScanDirectory(commonPrograms);
    }

    // 4. User Start Menu shortcuts
    wchar_t userPrograms[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userPrograms) == S_OK) {
        ScanDirectory(userPrograms);
    }

    CoUninitialize();
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

                // Resolve shortcut target to accurately inspect destination file
                std::wstring targetPath = fullPath;
                IShellLinkW* psl = nullptr;
                if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl)))) {
                    IPersistFile* ppf = nullptr;
                    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf)))) {
                        if (SUCCEEDED(ppf->Load(fullPath.c_str(), STGM_READ))) {
                            wchar_t szTarget[MAX_PATH] = { 0 };
                            if (SUCCEEDED(psl->GetPath(szTarget, MAX_PATH, nullptr, SLGP_UNCPRIORITY))) {
                                if (szTarget[0] != L'\0') {
                                    targetPath = szTarget;
                                }
                            }
                        }
                        ppf->Release();
                    }
                    psl->Release();
                }

                // Filter out non-applications when appsOnly is true
                if (!IsExcludedNonApp(name, targetPath, appsOnly)) {
                    AddApp(name, fullPath);
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

    // 1. Try direct launch (works for Win32 apps, .lnk, and system paths)
    HINSTANCE hRes = ShellExecuteW(nullptr, verb, app.path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hRes) > 32) return true;

    // 2. Try Shell AppsFolder AUMID (works for modern Store / UWP apps like WhatsApp)
    std::wstring shellTarget = (app.path.rfind(L"shell:AppsFolder\\", 0) == 0) ? app.path : (L"shell:AppsFolder\\" + app.path);
    HINSTANCE hResShell = ShellExecuteW(nullptr, verb, shellTarget.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hResShell) > 32) return true;

    // 3. Fallback: explorer.exe "shell:AppsFolder\..."
    HINSTANCE hResExp = ShellExecuteW(nullptr, nullptr, L"explorer.exe", shellTarget.c_str(), nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(hResExp) > 32;
}
