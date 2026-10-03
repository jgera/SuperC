@echo off
setlocal
echo ========================================================
echo   Registering C.exe into Windows Run (Win + R)
echo ========================================================

set "TARGET_EXE=%~dp0c.exe"
set "TARGET_DIR=%~dp0"
if "%TARGET_DIR:~-1%"=="\" set "TARGET_DIR=%TARGET_DIR:~0,-1%"

if not exist "%TARGET_EXE%" (
    echo [ERROR] c.exe not found in %~dp0. Please run build.bat first!
    pause
    exit /b 1
)

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\c.exe" /ve /t REG_SZ /d "%TARGET_EXE%" /f >nul
if errorlevel 1 (
    echo [ERROR] Failed to write to registry.
    pause
    exit /b 1
)

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\c.exe" /v Path /t REG_SZ /d "%TARGET_DIR%" /f >nul

echo [SUCCESS] c.exe has been registered!
echo.
echo You can now press Win + R and type:
echo   c 5+7
echo   c ipconfig
echo   c wifi
echo   c help
echo ========================================================
pause
