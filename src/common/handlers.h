#pragma once
#include <string>

namespace Handlers {

// Math & Calculations
bool HandleMath(const std::wstring& expr);
bool HandleHex(const std::wstring& args);
bool HandleTimestamp(const std::wstring& args);
bool HandlePassword(const std::wstring& args);

// Networking & Developer
bool HandleWifi(const std::wstring& args);
bool HandlePort(const std::wstring& args);
bool HandleKill(const std::wstring& args);
bool HandleIP(const std::wstring& args);
bool HandlePing(const std::wstring& args);

// Web & Search
bool HandleGoogle(const std::wstring& query);
bool HandleYouTube(const std::wstring& query);
bool HandleGitHub(const std::wstring& query);
bool HandleDirectUrl(const std::wstring& url);

// Clipboard utilities
bool HandleClipboardTransform(const std::wstring& action);

// Quick Folders & System
bool HandleFolders(const std::wstring& alias);
bool HandleHosts();
bool HandleEnv();

// Shell Execution
void LaunchShell(const std::wstring& command, bool elevated = false, const std::wstring& shell = L"cmd");

// Registration (App Paths)
void HandleInstall(bool uninstall = false);

} // namespace Handlers
