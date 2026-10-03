@echo off
setlocal
echo ========================================================
echo   Unregistering C.exe from Windows Run (Win + R)
echo ========================================================

reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\c.exe" /f >nul 2>&1

if errorlevel 1 (
    echo [INFO] c.exe was not registered or already removed.
) else (
    echo [SUCCESS] c.exe has been unregistered from Windows Run.
)

echo ========================================================
pause
