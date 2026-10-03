#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include "handlers.h"
#include "ui.h"
#include "math_eval.h"
#include <shellapi.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <psapi.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <ctime>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

namespace {

std::wstring UrlEncode(const std::wstring& str) {
    std::wostringstream escaped;
    for (wchar_t c : str) {
        if (iswalnum(c) || c == L'-' || c == L'_' || c == L'.' || c == L'~') {
            escaped << c;
        } else if (c == L' ') {
            escaped << L'+';
        } else {
            escaped << L'%' << std::uppercase << std::hex << std::setfill(L'0') << std::setw(2) << (int)(unsigned char)c;
        }
    }
    return escaped.str();
}

std::wstring RunHiddenCommand(const std::wstring& cmd) {
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return L"";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    std::wstring output;
    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWrite);
        hWrite = nullptr;

        char buffer[1024];
        DWORD bytesRead = 0;
        std::string rawOutput;
        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            rawOutput.append(buffer, bytesRead);
        }

        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        int wideLen = MultiByteToWideChar(CP_OEMCP, 0, rawOutput.c_str(), -1, nullptr, 0);
        if (wideLen > 0) {
            std::vector<wchar_t> wideBuf(wideLen);
            MultiByteToWideChar(CP_OEMCP, 0, rawOutput.c_str(), -1, wideBuf.data(), wideLen);
            output = wideBuf.data();
        }
    } else {
        if (hWrite) CloseHandle(hWrite);
    }

    if (hRead) CloseHandle(hRead);
    return output;
}

std::wstring GetSelfPath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

std::wstring GetSelfDir() {
    std::wstring path = GetSelfPath();
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(0, lastSlash);
    }
    return L"";
}

} // namespace

namespace Handlers {

bool HandleMath(const std::wstring& expr) {
    MathResult res = EvaluateMath(expr);
    if (!res.success) return false;

    DialogConfig cfg;
    cfg.title = L"SuperC - Calculator";
    cfg.prompt = L"Expression: " + expr;
    cfg.resultText = res.formatted;
    cfg.autoCopied = true;
    cfg.multiline = false;

    ShowResultDialog(cfg);
    return true;
}

bool HandleHex(const std::wstring& args) {
    std::wstring trimmed = args;
    size_t s = trimmed.find_first_not_of(L" \t");
    if (s != std::wstring::npos) trimmed = trimmed.substr(s);

    if (trimmed.empty()) return false;

    unsigned __int64 val = 0;
    wchar_t* endPtr = nullptr;
    if (trimmed.rfind(L"0x", 0) == 0 || trimmed.rfind(L"0X", 0) == 0) {
        val = _wcstoui64(trimmed.c_str(), &endPtr, 16);
    } else {
        val = _wcstoui64(trimmed.c_str(), &endPtr, 10);
    }

    std::wostringstream ossHex;
    ossHex << L"0x" << std::uppercase << std::hex << val;

    std::wstring binStr;
    unsigned __int64 temp = val;
    if (temp == 0) binStr = L"0";
    else {
        while (temp > 0) {
            binStr.insert(binStr.begin(), (temp & 1) ? L'1' : L'0');
            temp >>= 1;
        }
    }

    DialogConfig cfg;
    cfg.title = L"SuperC - Hex / Decimal Converter";
    cfg.prompt = L"Dec: " + std::to_wstring(val) + L"   |   Bin: 0b" + binStr;
    cfg.resultText = ossHex.str();
    cfg.autoCopied = true;
    cfg.multiline = false;

    ShowResultDialog(cfg);
    return true;
}

bool HandleTimestamp(const std::wstring& args) {
    std::wstring trimmed = args;
    size_t s = trimmed.find_first_not_of(L" \t");
    if (s != std::wstring::npos) trimmed = trimmed.substr(s);

    time_t rawTime = 0;
    if (trimmed.empty()) {
        rawTime = time(nullptr);
    } else {
        wchar_t* endPtr = nullptr;
        unsigned __int64 val = _wcstoui64(trimmed.c_str(), &endPtr, 10);
        if (val > 100000000000ULL) {
            // Milliseconds timestamp
            val /= 1000ULL;
        }
        rawTime = static_cast<time_t>(val);
    }

    struct tm timeinfo;
    localtime_s(&timeinfo, &rawTime);

    wchar_t buffer[80];
    wcsftime(buffer, sizeof(buffer)/sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S (%A)", &timeinfo);

    DialogConfig cfg;
    cfg.title = L"SuperC - Unix Timestamp";
    cfg.prompt = trimmed.empty() ? L"Current Timestamp: " + std::to_wstring(rawTime) : L"Timestamp: " + trimmed;
    cfg.resultText = buffer;
    cfg.autoCopied = true;
    cfg.multiline = false;

    ShowResultDialog(cfg);
    return true;
}

bool HandlePassword(const std::wstring& args) {
    int length = 16;
    if (!args.empty()) {
        int parsed = _wtoi(args.c_str());
        if (parsed >= 4 && parsed <= 128) {
            length = parsed;
        }
    }

    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*-_=+";
    const size_t charsetLen = sizeof(charset) - 1;

    HCRYPTPROV hCrypt = 0;
    std::wstring password;
    if (CryptAcquireContextW(&hCrypt, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        std::vector<BYTE> randomBytes(length);
        if (CryptGenRandom(hCrypt, length, randomBytes.data())) {
            for (int i = 0; i < length; i++) {
                password += static_cast<wchar_t>(charset[randomBytes[i] % charsetLen]);
            }
        }
        CryptReleaseContext(hCrypt, 0);
    }

    if (password.empty()) {
        password = L"C_PassGen_Error";
    }

    DialogConfig cfg;
    cfg.title = L"SuperC - Secure Password Generator";
    cfg.prompt = L"Generated " + std::to_wstring(length) + L"-character password:";
    cfg.resultText = password;
    cfg.autoCopied = true;
    cfg.multiline = false;

    ShowResultDialog(cfg);
    return true;
}

bool HandleWifi(const std::wstring& /*args*/) {
    // 1. Get active Wi-Fi profile name
    std::wstring ifaceOut = RunHiddenCommand(L"netsh wlan show interfaces");
    std::wstring ssid;
    std::wistringstream ss(ifaceOut);
    std::wstring line;
    while (std::getline(ss, line)) {
        size_t ssidPos = line.find(L"SSID");
        if (ssidPos != std::wstring::npos && line.find(L"BSSID") == std::wstring::npos) {
            size_t colon = line.find(L":", ssidPos);
            if (colon != std::wstring::npos) {
                std::wstring val = line.substr(colon + 1);
                size_t start = val.find_first_not_of(L" \t\r\n");
                size_t end = val.find_last_not_of(L" \t\r\n");
                if (start != std::wstring::npos && end != std::wstring::npos) {
                    ssid = val.substr(start, end - start + 1);
                    break;
                }
            }
        }
    }

    if (ssid.empty()) {
        DialogConfig cfg;
        cfg.title = L"SuperC - Wi-Fi";
        cfg.prompt = L"Status:";
        cfg.resultText = L"No connected Wi-Fi network detected.";
        cfg.autoCopied = false;
        ShowResultDialog(cfg);
        return true;
    }

    // 2. Query key content
    std::wstring cmd = L"netsh wlan show profile name=\"" + ssid + L"\" key=clear";
    std::wstring profOut = RunHiddenCommand(cmd);

    std::wstring password;
    std::wistringstream ssProf(profOut);
    while (std::getline(ssProf, line)) {
        size_t keyPos = line.find(L"Key Content");
        if (keyPos != std::wstring::npos) {
            size_t colon = line.find(L":", keyPos);
            if (colon != std::wstring::npos) {
                std::wstring val = line.substr(colon + 1);
                size_t start = val.find_first_not_of(L" \t\r\n");
                size_t end = val.find_last_not_of(L" \t\r\n");
                if (start != std::wstring::npos && end != std::wstring::npos) {
                    password = val.substr(start, end - start + 1);
                    break;
                }
            }
        }
    }

    DialogConfig cfg;
    cfg.title = L"SuperC - Wi-Fi Password";
    cfg.prompt = L"Network: " + ssid;
    if (!password.empty()) {
        cfg.resultText = password;
        cfg.autoCopied = true;
    } else {
        cfg.resultText = L"(Open network or password not saved)";
        cfg.autoCopied = false;
    }

    ShowResultDialog(cfg);
    return true;
}

bool HandlePort(const std::wstring& args) {
    int targetPort = _wtoi(args.c_str());
    if (targetPort <= 0 || targetPort > 65535) return false;

    // Allocate table for IPv4 TCP
    DWORD dwSize = 0;
    GetExtendedTcpTable(nullptr, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    std::vector<BYTE> buffer(dwSize);
    PMIB_TCPTABLE_OWNER_PID pTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());

    DWORD owningPid = 0;
    if (GetExtendedTcpTable(pTable, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pTable->dwNumEntries; i++) {
            int port = ntohs(static_cast<u_short>(pTable->table[i].dwLocalPort));
            if (port == targetPort) {
                owningPid = pTable->table[i].dwOwningPid;
                break;
            }
        }
    }

    DialogConfig cfg;
    cfg.title = L"SuperC - Port Inspector";
    if (owningPid == 0) {
        cfg.prompt = L"Port " + std::to_wstring(targetPort) + L":";
        cfg.resultText = L"FREE (No active process listening)";
        cfg.autoCopied = false;
    } else {
        std::wstring procName = L"Unknown Process";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, owningPid);
        if (hProc) {
            wchar_t fullPath[MAX_PATH] = { 0 };
            DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, fullPath, &len)) {
                std::wstring sPath = fullPath;
                size_t slash = sPath.find_last_of(L"\\/");
                if (slash != std::wstring::npos) {
                    procName = sPath.substr(slash + 1);
                } else {
                    procName = sPath;
                }
            }
            CloseHandle(hProc);
        }

        cfg.prompt = L"Port " + std::to_wstring(targetPort) + L" is in use:";
        cfg.resultText = procName + L" (PID: " + std::to_wstring(owningPid) + L")";
        cfg.autoCopied = true;
        cfg.secondaryButtonText = L"Kill Process";
        cfg.onSecondaryAction = [owningPid]() {
            HANDLE hKill = OpenProcess(PROCESS_TERMINATE, FALSE, owningPid);
            if (hKill) {
                TerminateProcess(hKill, 1);
                CloseHandle(hKill);
                MessageBoxW(nullptr, L"Process terminated successfully.", L"SuperC - Port", MB_OK | MB_ICONINFORMATION);
            } else {
                std::wstring cmd = L"/c taskkill /F /PID " + std::to_wstring(owningPid);
                ShellExecuteW(nullptr, L"runas", L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE);
            }
        };
    }

    ShowResultDialog(cfg);
    return true;
}

bool HandleKill(const std::wstring& args) {
    if (args.empty()) return false;
    std::wstring proc = args;
    if (proc.find(L".") == std::wstring::npos) {
        proc += L".exe";
    }
    std::wstring cmd = L"/c taskkill /F /IM " + proc;
    ShellExecuteW(nullptr, nullptr, L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE);
    return true;
}

bool HandleIP(const std::wstring& /*args*/) {
    ULONG outBufLen = 15000;
    std::vector<BYTE> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        dwRetVal = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    }

    std::wstring primaryIP;
    std::wostringstream oss;

    if (dwRetVal == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != nullptr; pCurr = pCurr->Next) {
            if (pCurr->OperStatus != IfOperStatusUp) continue;
            for (PIP_ADAPTER_UNICAST_ADDRESS pUni = pCurr->FirstUnicastAddress; pUni != nullptr; pUni = pUni->Next) {
                if (pUni->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(pUni->Address.lpSockaddr);
                    wchar_t ipStr[INET_ADDRSTRLEN];
                    InetNtopW(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
                    std::wstring ip(ipStr);
                    if (ip != L"127.0.0.1") {
                        if (primaryIP.empty()) primaryIP = ip;
                        oss << pCurr->FriendlyName << L": " << ip << L"\r\n";
                    }
                }
            }
        }
    }

    DialogConfig cfg;
    cfg.title = L"SuperC - Local IP Addresses";
    cfg.prompt = L"Active Adapters:";
    cfg.resultText = oss.str().empty() ? L"127.0.0.1" : oss.str();
    cfg.autoCopied = true;
    cfg.multiline = true;

    ShowResultDialog(cfg);
    return true;
}

bool HandlePing(const std::wstring& args) {
    if (args.empty()) return false;
    LaunchShell(L"ping " + args, false, L"cmd");
    return true;
}

bool HandleGoogle(const std::wstring& query) {
    std::wstring url = L"https://www.google.com/search?q=" + UrlEncode(query);
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return true;
}

bool HandleYouTube(const std::wstring& query) {
    std::wstring url = L"https://www.youtube.com/results?search_query=" + UrlEncode(query);
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return true;
}

bool HandleGitHub(const std::wstring& query) {
    std::wstring url;
    if (query.find(L"/") != std::wstring::npos && query.find(L" ") == std::wstring::npos) {
        url = L"https://github.com/" + query;
    } else {
        url = L"https://github.com/search?q=" + UrlEncode(query);
    }
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return true;
}

bool HandleDirectUrl(const std::wstring& url) {
    std::wstring fullUrl = url;
    if (fullUrl.rfind(L"http://", 0) != 0 && fullUrl.rfind(L"https://", 0) != 0) {
        fullUrl = L"http://" + fullUrl;
    }
    ShellExecuteW(nullptr, L"open", fullUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return true;
}

bool HandleClipboardTransform(const std::wstring& action) {
    std::wstring text = GetClipboardText();
    if (text.empty()) {
        DialogConfig cfg;
        cfg.title = L"SuperC - Clipboard";
        cfg.prompt = L"Status:";
        cfg.resultText = L"Clipboard is empty or contains non-text data.";
        cfg.autoCopied = false;
        ShowResultDialog(cfg);
        return true;
    }

    if (action == L"lower") {
        std::transform(text.begin(), text.end(), text.begin(), ::towlower);
        CopyToClipboard(text);
        DialogConfig cfg;
        cfg.title = L"SuperC - Clipboard Lowercase";
        cfg.prompt = L"Converted text to lowercase:";
        cfg.resultText = text;
        cfg.autoCopied = true;
        cfg.multiline = text.find(L"\n") != std::wstring::npos;
        ShowResultDialog(cfg);
        return true;
    } else if (action == L"upper") {
        std::transform(text.begin(), text.end(), text.begin(), ::towupper);
        CopyToClipboard(text);
        DialogConfig cfg;
        cfg.title = L"SuperC - Clipboard Uppercase";
        cfg.prompt = L"Converted text to uppercase:";
        cfg.resultText = text;
        cfg.autoCopied = true;
        cfg.multiline = text.find(L"\n") != std::wstring::npos;
        ShowResultDialog(cfg);
        return true;
    } else if (action == L"trim") {
        size_t first = text.find_first_not_of(L" \t\r\n");
        size_t last = text.find_last_not_of(L" \t\r\n");
        std::wstring trimmed = (first == std::wstring::npos) ? L"" : text.substr(first, last - first + 1);
        CopyToClipboard(trimmed);
        DialogConfig cfg;
        cfg.title = L"SuperC - Clipboard Trim";
        cfg.prompt = L"Trimmed text:";
        cfg.resultText = trimmed;
        cfg.autoCopied = true;
        ShowResultDialog(cfg);
        return true;
    } else if (action == L"count") {
        size_t chars = text.length();
        size_t lines = 1;
        size_t words = 0;
        bool inWord = false;
        for (wchar_t c : text) {
            if (c == L'\n') lines++;
            if (iswspace(c)) {
                inWord = false;
            } else if (!inWord) {
                inWord = true;
                words++;
            }
        }
        std::wostringstream oss;
        oss << L"Characters: " << chars << L"\r\nWords: " << words << L"\r\nLines: " << lines;

        DialogConfig cfg;
        cfg.title = L"SuperC - Clipboard Count";
        cfg.prompt = L"Clipboard Statistics:";
        cfg.resultText = oss.str();
        cfg.autoCopied = false;
        cfg.multiline = true;
        ShowResultDialog(cfg);
        return true;
    }

    return false;
}

bool HandleFolders(const std::wstring& alias) {
    if (alias == L"dl") {
        ShellExecuteW(nullptr, L"open", L"shell:Downloads", nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    } else if (alias == L"dt") {
        ShellExecuteW(nullptr, L"open", L"shell:Desktop", nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    } else if (alias == L"doc") {
        ShellExecuteW(nullptr, L"open", L"shell:Personal", nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    } else if (alias == L"temp") {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        ShellExecuteW(nullptr, L"open", tempPath, nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    } else if (alias == L"appdata") {
        wchar_t appData[MAX_PATH];
        SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData);
        ShellExecuteW(nullptr, L"open", appData, nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    } else if (alias == L"startup") {
        ShellExecuteW(nullptr, L"open", L"shell:startup", nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }
    return false;
}

bool HandleHosts() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring hostsPath = std::wstring(sysDir) + L"\\drivers\\etc\\hosts";
    ShellExecuteW(nullptr, L"runas", L"notepad.exe", hostsPath.c_str(), nullptr, SW_SHOWNORMAL);
    return true;
}

bool HandleEnv() {
    ShellExecuteW(nullptr, nullptr, L"rundll32.exe", L"sysdm.cpl,EditEnvironmentVariables", nullptr, SW_SHOWNORMAL);
    return true;
}

void LaunchShell(const std::wstring& command, bool elevated, const std::wstring& shell) {
    std::wstring prog;
    std::wstring params;

    if (shell == L"ps") {
        prog = L"powershell.exe";
        params = command.empty() ? L"-NoExit" : (L"-NoExit -Command \"" + command + L"\"");
    } else if (shell == L"wt") {
        prog = L"wt.exe";
        params = command.empty() ? L"" : (L"cmd /k \"" + command + L"\"");
    } else if (shell == L"pause") {
        prog = L"cmd.exe";
        params = L"/c \"" + command + L" & echo. & pause\"";
    } else {
        // Classic cmd
        prog = L"cmd.exe";
        params = command.empty() ? L"/k" : (L"/k \"" + command + L"\"");
    }

    const wchar_t* verb = elevated ? L"runas" : nullptr;
    ShellExecuteW(nullptr, verb, prog.c_str(), params.c_str(), nullptr, SW_SHOWNORMAL);
}

void HandleInstall(bool uninstall) {
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\c.exe";
    if (uninstall) {
        LSTATUS status = RegDeleteKeyW(HKEY_CURRENT_USER, subKey);
        DialogConfig cfg;
        cfg.title = L"SuperC - Uninstall";
        cfg.prompt = L"Registration Status:";
        cfg.resultText = (status == ERROR_SUCCESS) ? L"'c' was unregistered from Windows Run." : L"'c' was not registered or could not be removed.";
        cfg.autoCopied = false;
        ShowResultDialog(cfg);
    } else {
        HKEY hKey = nullptr;
        LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
        if (status == ERROR_SUCCESS) {
            std::wstring selfPath = GetSelfPath();
            std::wstring selfDir = GetSelfDir();
            RegSetValueExW(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(selfPath.c_str()), static_cast<DWORD>((selfPath.length() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hKey, L"Path", 0, REG_SZ, reinterpret_cast<const BYTE*>(selfDir.c_str()), static_cast<DWORD>((selfDir.length() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);

            DialogConfig cfg;
            cfg.title = L"SuperC - Installation Complete";
            cfg.prompt = L"'c' is now registered in Windows Run (Win + R)!";
            cfg.resultText = L"Location: " + selfPath;
            cfg.autoCopied = false;
            ShowResultDialog(cfg);
        } else {
            DialogConfig cfg;
            cfg.title = L"SuperC - Installation Error";
            cfg.prompt = L"Error:";
            cfg.resultText = L"Failed to write to registry key.";
            cfg.autoCopied = false;
            ShowResultDialog(cfg);
        }
    }
}

} // namespace Handlers
