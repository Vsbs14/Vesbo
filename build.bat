@echo off
setlocal

set "LIB_SOURCES=src\lexer.c src\ast.c src\parser.c src\value.c src\environment.c src\builtins.c src\raylib_bindings.c src\module.c src\interpreter.c src\bytecode.c src\codegen.c src\vm.c src\disassemble.c src\repl.c src\vesbo.c"
set "CLI_SOURCES=src\main.c"
set "SOURCES=%LIB_SOURCES% %CLI_SOURCES%"

for %%a in (%*) do (
    if /i "%%~a"=="--no-raylib" set NO_RAYLIB=1
    if /i "%%~a"=="/no-raylib" set NO_RAYLIB=1
    if /i "%%~a"=="-no-raylib" set NO_RAYLIB=1
)

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
if "%NO_RAYLIB%"=="1" (
    cl /MD /nologo /W4 /c /D_CRT_SECURE_NO_WARNINGS /DNO_RAYLIB /Iinclude %LIB_SOURCES%
    if errorlevel 1 exit /b 1
    lib /nologo /OUT:libvesbo.lib lexer.obj ast.obj parser.obj value.obj environment.obj builtins.obj raylib_bindings.obj module.obj interpreter.obj bytecode.obj codegen.obj vm.obj disassemble.obj repl.obj vesbo.obj
    if errorlevel 1 exit /b 1
    cl /MD /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /DNO_RAYLIB /Iinclude /Fe:vesbo.exe %CLI_SOURCES% libvesbo.lib /link /STACK:8388608
    if errorlevel 1 exit /b 1
    if exist tests\embed_host_test.c (
        cl /MD /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /DNO_RAYLIB /Iinclude /Fe:tests\embed_host_test.exe tests\embed_host_test.c libvesbo.lib /link /STACK:8388608
        if errorlevel 1 exit /b 1
    )
    del *.obj >nul 2>nul
    echo Built libvesbo.lib and vesbo.exe with MSVC ^(without Raylib^).
    exit /b 0
)
cl /MD /nologo /W4 /c /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ivendor\raylib\include %LIB_SOURCES%
if errorlevel 1 exit /b 1
lib /nologo /OUT:libvesbo.lib lexer.obj ast.obj parser.obj value.obj environment.obj builtins.obj raylib_bindings.obj module.obj interpreter.obj bytecode.obj codegen.obj vm.obj disassemble.obj repl.obj vesbo.obj
if errorlevel 1 exit /b 1
cl /MD /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ivendor\raylib\include /Fe:vesbo.exe %CLI_SOURCES% libvesbo.lib /link /STACK:8388608 /LIBPATH:vendor\raylib\lib raylib.lib user32.lib gdi32.lib winmm.lib shell32.lib
if errorlevel 1 exit /b 1
if exist tests\embed_host_test.c (
    cl /MD /nologo /W4 /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ivendor\raylib\include /Fe:tests\embed_host_test.exe tests\embed_host_test.c libvesbo.lib /link /STACK:8388608 /LIBPATH:vendor\raylib\lib raylib.lib user32.lib gdi32.lib winmm.lib shell32.lib
    if errorlevel 1 exit /b 1
)
del *.obj >nul 2>nul
if exist vendor\raylib\lib\raylib.dll if not exist raylib.dll copy /y vendor\raylib\lib\raylib.dll raylib.dll >nul
echo Built libvesbo.lib and vesbo.exe with MSVC and Raylib.
exit /b 0

:build_gcc
if "%NO_RAYLIB%"=="1" (
    gcc -Wall -Wextra -c -DNO_RAYLIB -Iinclude %LIB_SOURCES%
    if errorlevel 1 exit /b 1
    ar rcs libvesbo.a lexer.o ast.o parser.o value.o environment.o builtins.o raylib_bindings.o module.o interpreter.o bytecode.o codegen.o vm.o disassemble.o repl.o vesbo.o
    if errorlevel 1 exit /b 1
    gcc -Wall -Wextra -DNO_RAYLIB -Iinclude -o vesbo.exe %CLI_SOURCES% libvesbo.a -lm
    if errorlevel 1 exit /b 1
    if exist tests\embed_host_test.c (
        gcc -Wall -Wextra -DNO_RAYLIB -Iinclude -o tests\embed_host_test.exe tests\embed_host_test.c libvesbo.a -lm
        if errorlevel 1 exit /b 1
    )
    del *.o >nul 2>nul
    echo Built libvesbo.a and vesbo.exe with GCC ^(without Raylib^).
    exit /b 0
)
gcc -Wall -Wextra -c -Iinclude -Ivendor/raylib/include %LIB_SOURCES%
if errorlevel 1 exit /b 1
ar rcs libvesbo.a lexer.o ast.o parser.o value.o environment.o builtins.o raylib_bindings.o module.o interpreter.o bytecode.o codegen.o vm.o disassemble.o repl.o vesbo.o
if errorlevel 1 exit /b 1
gcc -Wall -Wextra -Iinclude -Ivendor/raylib/include -o vesbo.exe %CLI_SOURCES% libvesbo.a -Lvendor/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lm
if errorlevel 1 exit /b 1
if exist tests\embed_host_test.c (
    gcc -Wall -Wextra -Iinclude -Ivendor/raylib/include -o tests\embed_host_test.exe tests\embed_host_test.c libvesbo.a -Lvendor/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lm
    if errorlevel 1 exit /b 1
)
del *.o >nul 2>nul
if exist vendor\raylib\lib\raylib.dll if not exist raylib.dll copy /y vendor\raylib\lib\raylib.dll raylib.dll >nul
echo Built libvesbo.a and vesbo.exe with GCC and Raylib.
exit /b 0

:build_clang
if "%NO_RAYLIB%"=="1" (
    clang -Wall -Wextra -c -DNO_RAYLIB -Iinclude %LIB_SOURCES%
    if errorlevel 1 exit /b 1
    llvm-ar rcs libvesbo.a lexer.o ast.o parser.o value.o environment.o builtins.o raylib_bindings.o module.o interpreter.o bytecode.o codegen.o vm.o disassemble.o repl.o vesbo.o 2>nul || ar rcs libvesbo.a lexer.o ast.o parser.o value.o environment.o builtins.o raylib_bindings.o module.o interpreter.o bytecode.o codegen.o vm.o disassemble.o repl.o vesbo.o
    if errorlevel 1 exit /b 1
    clang -Wall -Wextra -DNO_RAYLIB -Iinclude -o vesbo.exe %CLI_SOURCES% libvesbo.a -lm
    if errorlevel 1 exit /b 1
    if exist tests\embed_host_test.c (
        clang -Wall -Wextra -DNO_RAYLIB -Iinclude -o tests\embed_host_test.exe tests\embed_host_test.c libvesbo.a -lm
        if errorlevel 1 exit /b 1
    )
    del *.o >nul 2>nul
    echo Built libvesbo.a and vesbo.exe with Clang ^(without Raylib^).
    exit /b 0
)
clang -Wall -Wextra -c -Iinclude -Ivendor/raylib/include %LIB_SOURCES%
if errorlevel 1 exit /b 1
llvm-ar rcs libvesbo.a lexer.o ast.o parser.o value.o environment.o builtins.o raylib_bindings.o module.o interpreter.o bytecode.o codegen.o vm.o disassemble.o repl.o vesbo.o 2>nul || ar rcs libvesbo.a lexer.o ast.o parser.o value.o environment.o builtins.o raylib_bindings.o module.o interpreter.o bytecode.o codegen.o vm.o disassemble.o repl.o vesbo.o
if errorlevel 1 exit /b 1
clang -Wall -Wextra -Iinclude -Ivendor/raylib/include -o vesbo.exe %CLI_SOURCES% libvesbo.a -Lvendor/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lm
if errorlevel 1 exit /b 1
if exist tests\embed_host_test.c (
    clang -Wall -Wextra -Iinclude -Ivendor/raylib/include -o tests\embed_host_test.exe tests\embed_host_test.c libvesbo.a -Lvendor/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lm
    if errorlevel 1 exit /b 1
)
del *.o >nul 2>nul
if exist vendor\raylib\lib\raylib.dll if not exist raylib.dll copy /y vendor\raylib\lib\raylib.dll raylib.dll >nul
echo Built libvesbo.a and vesbo.exe with Clang and Raylib.
exit /b 0