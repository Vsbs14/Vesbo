@echo off
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if %ERRORLEVEL%==0 (
    cl /nologo /W3 /Iinclude src\lexer.c src\ast.c src\parser.c src\value.c src\environment.c src\builtins.c src\module.c src\interpreter.c src\bytecode.c src\codegen.c src\vm.c src\disassemble.c src\repl.c src\main.c /Fe:vesbo.exe
    if %ERRORLEVEL%==0 (
        del *.obj >nul 2>nul
        exit /b 0
    )
)

where gcc >nul 2>nul
if %ERRORLEVEL%==0 (
    gcc -Wall -Wextra -Iinclude src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c src/builtins.c src/module.c src/interpreter.c src/bytecode.c src/codegen.c src/vm.c src/disassemble.c src/repl.c src/main.c -o vesbo.exe -lm
    if %ERRORLEVEL%==0 exit /b 0
)

where clang >nul 2>nul
if %ERRORLEVEL%==0 (
    clang -Wall -Wextra -Iinclude src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c src/builtins.c src/module.c src/interpreter.c src/bytecode.c src/codegen.c src/vm.c src/disassemble.c src/repl.c src/main.c -o vesbo.exe -lm
    if %ERRORLEVEL%==0 exit /b 0
)

exit /b 1
