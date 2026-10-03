@echo off
setlocal

echo ===================================================
echo   Building C.exe (Native C++ Windows Utility)
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

echo Compiling source files...
cl.exe /nologo /O2 /W4 /EHsc /std:c++17 /DUNICODE /D_UNICODE ^
    /Fe:bin\c.exe ^
    /Fo:bin\ ^
    src\main.cpp src\math_eval.cpp src\ui.cpp src\handlers.cpp ^
    /link /SUBSYSTEM:WINDOWS ^
    user32.lib gdi32.lib shell32.lib iphlpapi.lib ws2_32.lib advapi32.lib crypt32.lib comctl32.lib

if errorlevel 1 (
    echo [ERROR] Compilation failed.
    exit /b 1
)

copy /y bin\c.exe c.exe >nul
echo.
echo [SUCCESS] c.exe built successfully!
echo Binary output: bin\c.exe (and .\c.exe)
echo.
dir c.exe | findstr "c.exe"
echo ===================================================
endlocal
