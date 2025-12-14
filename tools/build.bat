@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "BASH_SCRIPT=%SCRIPT_DIR%build.sh"

if exist "C:\msys64\usr\bin\bash.exe" (
    "C:\msys64\usr\bin\bash.exe" "%BASH_SCRIPT%" %*
    exit /b %errorlevel%
)

if exist "C:\Program Files\Git\bin\bash.exe" (
    "C:\Program Files\Git\bin\bash.exe" "%BASH_SCRIPT%" %*
    exit /b %errorlevel%
)

where bash >nul 2>&1
if %errorlevel% equ 0 (
    bash "%BASH_SCRIPT%" %*
    exit /b %errorlevel%
)

echo ERROR: bash not found. Install MSYS2 or Git for Windows.
exit /b 1
