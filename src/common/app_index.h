#pragma once
#include <string>
#include <vector>

struct AppEntry {
    std::wstring name;
    std::wstring path;
    int score = 0;
};

class AppIndexer {
public:
    static AppIndexer& Instance();

    // Scans Start Menu .lnk shortcuts into memory (fast, < 5ms)
    void RefreshIndex();

    // Searches indexed apps with fuzzy / subsequence matching
    std::vector<AppEntry> Search(const std::wstring& query, size_t maxResults = 5);

    // Launches the selected application
    static bool Launch(const AppEntry& app);

private:
    AppIndexer();
    std::vector<AppEntry> apps;
    bool initialized = false;

    void ScanDirectory(const std::wstring& dir);
};
