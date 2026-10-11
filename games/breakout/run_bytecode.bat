@echo off
cd /d "%~dp0\..\.."
if not exist "vesbo.exe" (
    call build.bat
    if errorlevel 1 exit /b 1
)
vesbo.exe -c games\breakout\breakout.vsb -o games\breakout\breakout.vbo
if errorlevel 1 exit /b 1
vesbo.exe games\breakout\breakout.vbo
