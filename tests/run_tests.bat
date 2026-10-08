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

REM Test all features in tree-walking interpreter
"%ROOT%\vesbo.exe" "%~dp0all_features_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"apple - banana - orange" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"bonono" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Handled: division by zero" "%OUTPUT%" >nul || goto :failed

REM Test compiling and executing all features in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0all_features_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"apple - banana - orange" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"bonono" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Handled: division by zero" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test new syntax (fnc, quotes, loop while, <=) in interpreter
"%ROOT%\vesbo.exe" "%~dp0new_syntax_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"hello world" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"hello single" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"hash # string" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Hello, Alice!" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"15" "%OUTPUT%" >nul || goto :failed

REM Test new syntax in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0new_syntax_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"hello world" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"hello single" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"hash # string" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Hello, Alice!" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"15" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test disassembler on .vsb source
"%ROOT%\vesbo.exe" -d "%ROOT%\examples\array_loops.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /c:"Bytecode Disassembly" "%OUTPUT%" >nul || goto :failed
findstr /c:"OP_PUSH_ARRAY" "%OUTPUT%" >nul || goto :failed
findstr /c:"OP_HALT" "%OUTPUT%" >nul || goto :failed

REM Test disassembler on .vbo bytecode
"%ROOT%\vesbo.exe" -c "%ROOT%\examples\array_loops.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" -d "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /c:"Bytecode Disassembly" "%OUTPUT%" >nul || goto :failed
findstr /c:"OP_PUSH_ARRAY" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test REPL expression evaluation via pipe
echo 21 * 2 | "%ROOT%\vesbo.exe" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"42" "%OUTPUT%" >nul || goto :failed

REM Test CLI flags
"%ROOT%\vesbo.exe" --version > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /c:"Vesbo 0.2.0" "%OUTPUT%" >nul || goto :failed

"%ROOT%\vesbo.exe" --help > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /c:"Usage:" "%OUTPUT%" >nul || goto :failed
findstr /c:"--disassemble" "%OUTPUT%" >nul || goto :failed
findstr /c:"--repl" "%OUTPUT%" >nul || goto :failed

del "%OUTPUT%" >nul 2>nul
echo All Vesbo smoke tests passed.
exit /b 0

:failed
del "%OUTPUT%" >nul 2>nul
echo Vesbo smoke tests failed.
exit /b 1