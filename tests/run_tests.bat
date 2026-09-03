@echo off
setlocal
set "ROOT=%~dp0.."
set "OUTPUT=%TEMP%\vesbo-tests-%RANDOM%.txt"

if not exist "%ROOT%\vesbo.exe" (
    call "%ROOT%\build.bat"
    if errorlevel 1 exit /b 1
)

"%ROOT%\vesbo.exe" "%~dp0runtime_regressions.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"7" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"20" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"number(): expected a valid number" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"array index must be an integer" "%OUTPUT%" >nul || goto :failed

"%ROOT%\vesbo.exe" "%ROOT%\examples\array_loops.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"108" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"18" "%OUTPUT%" >nul || goto :failed

"%ROOT%\vesbo.exe" "%ROOT%\examples\try_catch_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"before error" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"division by zero" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"after try/catch" "%OUTPUT%" >nul || goto :failed

del "%OUTPUT%" >nul 2>nul
echo All Vesbo smoke tests passed.
exit /b 0

:failed
del "%OUTPUT%" >nul 2>nul
echo Vesbo smoke tests failed.
exit /b 1