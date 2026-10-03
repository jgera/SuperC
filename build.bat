@echo off
setlocal

echo ===================================================
echo   Building SuperC (CLI and Quick Launcher)
echo ===================================================

set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VS_VCVARS%" (
    echo [ERROR] Visual Studio 2022 vcvars64.bat not found at:
    echo "%VS_VCVARS%"
    exit /b 1
)

call "%VS_VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC build environment.
    exit /b 1
)

if not exist "bin" mkdir "bin"

set "COMMON_FLAGS=/nologo /O2 /W4 /EHsc /std:c++17 /DUNICODE /D_UNICODE /Isrc\common /Isrc\cli /Isrc\launcher"
set "COMMON_LIBS=user32.lib gdi32.lib shell32.lib iphlpapi.lib ws2_32.lib advapi32.lib crypt32.lib comctl32.lib dwmapi.lib ole32.lib"

echo [1/2] Compiling c.exe (Win+R / Command-Line Tool)...
cl.exe %COMMON_FLAGS% ^
    /Fe:bin\c.exe ^
    /Fo:bin\ ^
    src\cli\main_cli.cpp src\cli\ui.cpp ^
    src\common\math_eval.cpp src\common\handlers.cpp ^
    /link /SUBSYSTEM:WINDOWS %COMMON_LIBS%

if errorlevel 1 (
    echo [ERROR] Failed to build c.exe
    exit /b 1
)
copy /y bin\c.exe c.exe >nul

echo.
echo [2/2] Compiling SuperC-Launcher.exe (Floating Quick Launcher)...
cl.exe %COMMON_FLAGS% ^
    /Fe:bin\SuperC-Launcher.exe ^
    /Fo:bin\ ^
    src\launcher\main_launcher.cpp src\launcher\launcher_ui.cpp ^
    src\cli\ui.cpp ^
    src\common\math_eval.cpp src\common\unit_conv.cpp src\common\app_index.cpp ^
    src\common\window_walker.cpp src\common\sys_control.cpp src\common\handlers.cpp ^
    /link /SUBSYSTEM:WINDOWS %COMMON_LIBS%

if errorlevel 1 (
    echo [ERROR] Failed to build SuperC-Launcher.exe
    exit /b 1
)
copy /y bin\SuperC-Launcher.exe SuperC-Launcher.exe >nul

echo.
echo ===================================================
echo [SUCCESS] Both binaries built successfully!
echo   1. c.exe (Win+R / CLI Tool)
echo   2. SuperC-Launcher.exe (Ctrl+Space Quick Launcher)
echo ===================================================
endlocal
