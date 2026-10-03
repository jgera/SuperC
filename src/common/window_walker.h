#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct WindowEntry {
    HWND hwnd = nullptr;
    std::wstring title;
    std::wstring processName;
    int score = 0;
};

class WindowWalker {
public:
    // Finds open windows matching query
    static std::vector<WindowEntry> SearchOpenWindows(const std::wstring& query);

    // Switches focus to the given window
    static bool SwitchTo(HWND hwnd);
};
