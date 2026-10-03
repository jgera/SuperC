#include <windows.h>
#include <string>
#include "math_eval.h"
#include "ui.h"
#include "handlers.h"

namespace {

std::wstring GetTailArgs() {
    const wchar_t* p = GetCommandLineW();
    if (!p) return L"";

    // Skip leading whitespace
    while (*p == L' ' || *p == L'\t') p++;

    // Skip executable token
    if (*p == L'"') {
        p++;
        while (*p && *p != L'"') p++;
        if (*p == L'"') p++;
    } else {
        while (*p && *p != L' ' && *p != L'\t') p++;
    }

    // Skip whitespace between exe and arguments
    while (*p == L' ' || *p == L'\t') p++;

    return p;
}

std::wstring ToLower(const std::wstring& str) {
    std::wstring res = str;
    for (auto& c : res) {
        c = towlower(c);
    }
    return res;
}

} // namespace

int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    // Enable DPI awareness for crisp rendering
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    std::wstring tail = GetTailArgs();

    // Trim trailing whitespace
    size_t lastNonSpace = tail.find_last_not_of(L" \t\r\n");
    if (lastNonSpace != std::wstring::npos) {
        tail = tail.substr(0, lastNonSpace + 1);
    } else {
        tail.clear();
    }

    // 1. If run with no arguments (e.g. typing just 'c' in Run dialog)
    if (tail.empty()) {
        Handlers::LaunchShell(L"", false, L"cmd");
        return 0;
    }

    // 2. Help dialog
    std::wstring lowerTail = ToLower(tail);
    if (lowerTail == L"help" || lowerTail == L"--help" || lowerTail == L"-h" || lowerTail == L"/?" || lowerTail == L"?") {
        ShowHelpDialog();
        return 0;
    }

    // 3. Register / Install in Windows Run
    if (lowerTail == L"install" || lowerTail == L"register") {
        Handlers::HandleInstall(false);
        return 0;
    }
    if (lowerTail == L"uninstall" || lowerTail == L"unregister") {
        Handlers::HandleInstall(true);
        return 0;
    }

    // 4. Math expression evaluation
    if (LooksLikeMath(tail)) {
        if (Handlers::HandleMath(tail)) {
            return 0;
        }
    }

    // 5. Parse verb and parameters
    size_t firstSpace = tail.find_first_of(L" \t");
    std::wstring verb = (firstSpace == std::wstring::npos) ? tail : tail.substr(0, firstSpace);
    std::wstring args = (firstSpace == std::wstring::npos) ? L"" : tail.substr(firstSpace + 1);

    // Trim args leading whitespace
    size_t argsStart = args.find_first_not_of(L" \t");
    if (argsStart != std::wstring::npos) {
        args = args.substr(argsStart);
    } else {
        args.clear();
    }

    std::wstring lowerVerb = ToLower(verb);

    // Elevation (# or sudo)
    if (lowerVerb == L"#" || lowerVerb == L"sudo") {
        Handlers::LaunchShell(args, true, L"cmd");
        return 0;
    }

    // Shell switchers
    if (lowerVerb == L"ps") {
        Handlers::LaunchShell(args, false, L"ps");
        return 0;
    }
    if (lowerVerb == L"wt") {
        Handlers::LaunchShell(args, false, L"wt");
        return 0;
    }
    if (lowerVerb == L"-p" || lowerVerb == L"/p") {
        Handlers::LaunchShell(args, false, L"pause");
        return 0;
    }

    // Developer & Networking tools
    if (lowerVerb == L"port") {
        if (Handlers::HandlePort(args)) return 0;
    }
    if (lowerVerb == L"kill") {
        if (Handlers::HandleKill(args)) return 0;
    }
    if (lowerVerb == L"pass") {
        if (Handlers::HandlePassword(args)) return 0;
    }
    if (lowerVerb == L"wifi") {
        if (Handlers::HandleWifi(args)) return 0;
    }
    if (lowerVerb == L"ip" || lowerVerb == L"myip") {
        if (Handlers::HandleIP(args)) return 0;
    }
    if (lowerVerb == L"p" && !args.empty()) {
        if (Handlers::HandlePing(args)) return 0;
    }

    // Converters
    if (lowerVerb == L"hex") {
        if (Handlers::HandleHex(args)) return 0;
    }
    if (lowerVerb == L"ts") {
        if (Handlers::HandleTimestamp(args)) return 0;
    }

    // Clipboard transforms
    if (lowerVerb == L"lower" || lowerVerb == L"upper" || lowerVerb == L"trim" || lowerVerb == L"count") {
        if (Handlers::HandleClipboardTransform(lowerVerb)) return 0;
    }

    // Web searches & URLs
    if (lowerVerb == L"g" && !args.empty()) {
        if (Handlers::HandleGoogle(args)) return 0;
    }
    if (lowerVerb == L"yt" && !args.empty()) {
        if (Handlers::HandleYouTube(args)) return 0;
    }
    if (lowerVerb == L"gh" && !args.empty()) {
        if (Handlers::HandleGitHub(args)) return 0;
    }
    if (tail.rfind(L"http://", 0) == 0 || tail.rfind(L"https://", 0) == 0 ||
        tail.rfind(L"localhost", 0) == 0 || tail.rfind(L"www.", 0) == 0) {
        if (Handlers::HandleDirectUrl(tail)) return 0;
    }

    // Quick folders & System tools
    if (lowerVerb == L"dl" || lowerVerb == L"dt" || lowerVerb == L"doc" ||
        lowerVerb == L"temp" || lowerVerb == L"appdata" || lowerVerb == L"startup") {
        if (Handlers::HandleFolders(lowerVerb)) return 0;
    }
    if (lowerVerb == L"hosts") {
        if (Handlers::HandleHosts()) return 0;
    }
    if (lowerVerb == L"env") {
        if (Handlers::HandleEnv()) return 0;
    }

    // 6. Default Fallback: Run whatever command was provided in the shell!
    // Keeps terminal open (cmd /k) so output is completely readable.
    Handlers::LaunchShell(tail, false, L"cmd");
    return 0;
}
