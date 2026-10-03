#include "sys_control.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>

namespace {

std::wstring ToLower(const std::wstring& str) {
    std::wstring res = str;
    for (auto& c : res) c = towlower(c);
    return res;
}

std::wstring Trim(const std::wstring& str) {
    size_t s = str.find_first_not_of(L" \t\r\n");
    if (s == std::wstring::npos) return L"";
    size_t e = str.find_last_not_of(L" \t\r\n");
    return str.substr(s, e - s + 1);
}

} // namespace

bool SysControl::Match(const std::wstring& rawQuery, std::wstring& outDescription) {
    std::wstring q = ToLower(Trim(rawQuery));
    if (q.empty()) return false;

    if (q == L"lock") {
        outDescription = L"Lock Workstation";
        return true;
    }
    if (q == L"sleep") {
        outDescription = L"Put Computer to Sleep";
        return true;
    }
    if (q == L"restart" || q == L"reboot") {
        outDescription = L"Restart Computer";
        return true;
    }
    if (q == L"shutdown") {
        outDescription = L"Shut Down Computer";
        return true;
    }
    if (q == L"empty trash" || q == L"empty recycle bin" || q == L"trash") {
        outDescription = L"Empty Recycle Bin";
        return true;
    }

    return false;
}

bool SysControl::Execute(const std::wstring& rawQuery) {
    std::wstring q = ToLower(Trim(rawQuery));
    if (q == L"lock") {
        return LockWorkStation() != 0;
    }
    if (q == L"sleep") {
        ShellExecuteW(nullptr, nullptr, L"rundll32.exe", L"powrprof.dll,SetSuspendState 0,1,0", nullptr, SW_HIDE);
        return true;
    }
    if (q == L"restart" || q == L"reboot") {
        ShellExecuteW(nullptr, nullptr, L"shutdown.exe", L"/r /t 0", nullptr, SW_HIDE);
        return true;
    }
    if (q == L"shutdown") {
        ShellExecuteW(nullptr, nullptr, L"shutdown.exe", L"/s /t 0", nullptr, SW_HIDE);
        return true;
    }
    if (q == L"empty trash" || q == L"empty recycle bin" || q == L"trash") {
        SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        return true;
    }

    return false;
}
