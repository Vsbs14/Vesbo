@echo off
setlocal

set "SOURCES=src\lexer.c src\ast.c src\parser.c src\value.c src\environment.c src\builtins.c src\module.c src\interpreter.c src\bytecode.c src\codegen.c src\vm.c src\disassemble.c src\repl.c src\main.c"

where cl >nul 2>nul
if not errorlevel 1 goto :build_msvc

where gcc >nul 2>nul
if not errorlevel 1 goto :build_gcc

where clang >nul 2>nul
if not errorlevel 1 goto :build_clang

REM Try locating Visual Studio via vswhere if cl is not in PATH
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_DIR=%%i"
    )
)
if defined VS_DIR (
    if exist "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        goto :build_msvc
    )
    if exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" (
        call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
        goto :build_msvc
    )
)

echo No supported C compiler found.
echo Install Visual Studio Build Tools, MinGW-w64, or LLVM Clang.
exit /b 1

:build_msvc
cl /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Fe:vesbo.exe %SOURCES%
if errorlevel 1 exit /b 1
del *.obj >nul 2>nul
echo Built vesbo.exe with MSVC.
exit /b 0

:build_gcc
gcc -Wall -Wextra -Iinclude -o vesbo.exe %SOURCES%
if errorlevel 1 exit /b 1
echo Built vesbo.exe with GCC.
exit /b 0

:build_clang
clang -Wall -Wextra -Iinclude -o vesbo.exe %SOURCES%
if errorlevel 1 exit /b 1
echo Built vesbo.exe with Clang.
exit /b 0