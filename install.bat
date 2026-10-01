@echo off
title Shogun Sync Installation
echo ==========================================
echo       Installation Shogun Sync Tool
echo ==========================================
echo.

set "INSTALL_DIR=%LOCALAPPDATA%\ShogunSync"
set "SAVE_DIR=%LOCALAPPDATA%\ShogunSync\saves"

if not exist "%INSTALL_DIR%" (
    echo Creating directory %INSTALL_DIR%...
    mkdir "%INSTALL_DIR%"
)

if not exist "%SAVE_DIR%" (
    echo Creating directory %SAVE_DIR%...
    mkdir "%SAVE_DIR%"
)

echo Files copying...
copy /Y "%~dp0shogun-sync.exe" "%INSTALL_DIR%\shogun-sync.exe" >nul

if errorlevel 1 (
    echo.
    echo [ERROR] shogun-sync.exe NOT FOUND
    echo Make sure that you have .exe file in same directory as .bat
    pause
    exit /b 1
)

echo Installation...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$userPath = [Environment]::GetEnvironmentVariable('PATH', 'User'); if ($userPath -notmatch [regex]::Escape('%INSTALL_DIR%')) { [Environment]::SetEnvironmentVariable('PATH', $userPath + ';%INSTALL_DIR%', 'User'); Write-Host 'New Path added' } else { Write-Host 'Path already exists, step skipped.' }"

echo.
echo ==========================================
echo   Success! Shogun Sync was installed correctly
echo ==========================================
echo.
echo Close all opened terminals
echo In new terminal you can use: shogun-sync
echo.
pause