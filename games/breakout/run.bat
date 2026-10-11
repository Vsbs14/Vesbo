@echo off
cd /d "%~dp0\..\.."
if not exist "vesbo.exe" (
    call build.bat
    if errorlevel 1 exit /b 1
)
vesbo.exe games\breakout\breakout.vsb
