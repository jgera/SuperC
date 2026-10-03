#pragma once
#include <string>

struct SysCommand {
    std::wstring keyword;
    std::wstring description;
};

class SysControl {
public:
    static bool Match(const std::wstring& query, std::wstring& outDescription);
    static bool Execute(const std::wstring& query);
};
