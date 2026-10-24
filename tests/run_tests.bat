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

REM Test maps and index mutation in interpreter
"%ROOT%\vesbo.exe" "%~dp0map_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"100" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"999" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"25" "%OUTPUT%" >nul || goto :failed

REM Test maps and index mutation in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0map_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"100" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"999" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"25" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test File and OS built-ins in interpreter
"%ROOT%\vesbo.exe" "%~dp0io_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"Hello Vesbo I/O! Extra content." "%OUTPUT%" >nul || goto :failed
findstr /x /c:"none" "%OUTPUT%" >nul || goto :failed

REM Test File and OS built-ins in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0io_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"Hello Vesbo I/O! Extra content." "%OUTPUT%" >nul || goto :failed
findstr /x /c:"none" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test Modules and imports in interpreter
"%ROOT%\vesbo.exe" "%~dp0import_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"import test success" "%OUTPUT%" >nul || goto :failed

REM Test Modules and imports in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0import_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"import test success" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test Circular imports in interpreter
"%ROOT%\vesbo.exe" "%~dp0circ_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"circular import success" "%OUTPUT%" >nul || goto :failed

REM Test Circular imports in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0circ_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"circular import success" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test String interpolation in interpreter
"%ROOT%\vesbo.exe" "%~dp0interpolation_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"interpolation test success" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Hello, World!" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"User: Alice, age: 30" "%OUTPUT%" >nul || goto :failed

REM Test String interpolation in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0interpolation_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"interpolation test success" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Hello, World!" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"User: Alice, age: 30" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test Standard Library (math, strings, lists) in interpreter
"%ROOT%\vesbo.exe" "%~dp0stdlib_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"stdlib test success" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"reversed: 20, 15, 10, 5" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"pad_left: 007" "%OUTPUT%" >nul || goto :failed

REM Test Standard Library (math, strings, lists) in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0stdlib_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"stdlib test success" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"reversed: 20, 15, 10, 5" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"pad_left: 007" "%OUTPUT%" >nul || goto :failed
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

REM Test Bare module imports in interpreter
"%ROOT%\vesbo.exe" "%~dp0bare_import_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"300" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"bare import success" "%OUTPUT%" >nul || goto :failed

REM Test Bare module imports in bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0bare_import_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"300" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"bare import success" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test Showcase example in interpreter
"%ROOT%\vesbo.exe" "%ROOT%\examples\showcase.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"FizzBuzz" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Hello, Vesbo!" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"The answer is: 42" "%OUTPUT%" >nul || goto :failed

REM Test Showcase example in bytecode VM
"%ROOT%\vesbo.exe" -c "%ROOT%\examples\showcase.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"FizzBuzz" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"Hello, Vesbo!" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"The answer is: 42" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test Calculator example in interpreter
(echo add& echo 10& echo 5& echo quit) | "%ROOT%\vesbo.exe" "%ROOT%\examples\calculator.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"15" "%OUTPUT%" >nul || goto :failed

REM Test Calculator example in bytecode VM
"%ROOT%\vesbo.exe" -c "%ROOT%\examples\calculator.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
(echo add& echo 10& echo 5& echo quit) | "%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"15" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test Milestones L1-L6 in tree-walking interpreter
"%ROOT%\vesbo.exe" "%~dp0milestones_l1_l6_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"L1_arr_ref:99:7" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L1_map_ref:250:10" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L1_struct_ref:30:3" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L1_gc_ran:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L2_globals:15:40" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L2_map_sum:89700" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L2_deep_stack:350" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L3_dot_assign:10:20" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L3_method_pan:32:514" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L3_defaults:3:2:5:61" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_status_init:suspended:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_coro_vals:30:150:27" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_status_done:dead:false" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_nested_coro:30:50" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_coro_struct:25:100" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_nested_catch:110:dead" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_bad_start_status:dead" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L6_trace:true:true:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L6_profile:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"MILESTONES_L1_L6_PASS" "%OUTPUT%" >nul || goto :failed

REM Test Milestones L1-L6 in compiled .vbo bytecode VM
"%ROOT%\vesbo.exe" -c "%~dp0milestones_l1_l6_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"L1_arr_ref:99:7" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L1_map_ref:250:10" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L1_struct_ref:30:3" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L1_gc_ran:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L2_globals:15:40" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L2_map_sum:89700" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L2_deep_stack:350" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L3_dot_assign:10:20" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L3_method_pan:32:514" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L3_defaults:3:2:5:61" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_status_init:suspended:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_coro_vals:30:150:27" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_status_done:dead:false" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_nested_coro:30:50" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_coro_struct:25:100" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_nested_catch:110:dead" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L4_bad_start_status:dead" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L6_trace:true:true:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"L6_profile:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"MILESTONES_L1_L6_PASS" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test Milestones L1-L6 under --gc-stress and --profile
"%ROOT%\vesbo.exe" --gc-stress --profile --vm "%~dp0milestones_l1_l6_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"MILESTONES_L1_L6_PASS" "%OUTPUT%" >nul || goto :failed
findstr /c:"[Profile] Instructions:" "%OUTPUT%" >nul || goto :failed

"%ROOT%\vesbo.exe" --gc-stress "%~dp0milestones_l1_l6_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"MILESTONES_L1_L6_PASS" "%OUTPUT%" >nul || goto :failed

REM Test Milestone L5 C Embedding API host program
if not exist "%~dp0embed_host_test.exe" (
    call "%ROOT%\build.bat"
    if errorlevel 1 goto :failed
)
"%~dp0embed_host_test.exe" "%~dp0embed_script.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"tick1 status=1 val=16" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"tick2 status=1 val=35" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"tick3 status=0 val=1035" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"host_ticks=102" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"EMBED_HOST_PASS" "%OUTPUT%" >nul || goto :failed

REM Test 30: New features audit in interpreter
"%ROOT%\vesbo.exe" "%~dp0new_features_audit_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"while_break_cont:18" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"till_break_cont:13" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"range_break_cont:9" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"through_break_cont:ab" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"var_compound:26" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"str_compound:hello world" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"struct_compound:15:60" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"array_compound:25:60" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"map_compound:25" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"str_slice_basic:bcd" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"str_slice_no_end:cdef" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"str_slice_neg_start:def" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"str_slice_neg_end:bcde" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"str_substring:abc" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"arr_slice_basic:3:20:40" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"arr_slice_neg:2:40:50" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"safe_and:false" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"safe_or:true" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"split_multichar:3:foo:bar:baz" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"rejoined:foo - bar - baz" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"math_clamp:10" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"math_sign:-1" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"math_pow:32" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"AUDIT_TESTS_PASS" "%OUTPUT%" >nul || goto :failed

REM Test 30: New features audit in direct bytecode VM
"%ROOT%\vesbo.exe" --vm "%~dp0new_features_audit_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"while_break_cont:18" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"AUDIT_TESTS_PASS" "%OUTPUT%" >nul || goto :failed

REM Test 30: New features audit in serialized bytecode
"%ROOT%\vesbo.exe" -c "%~dp0new_features_audit_test.vsb" -o "%OUTPUT%.vbo" >nul 2>&1
if errorlevel 1 goto :failed
"%ROOT%\vesbo.exe" "%OUTPUT%.vbo" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"while_break_cont:18" "%OUTPUT%" >nul || goto :failed
findstr /x /c:"AUDIT_TESTS_PASS" "%OUTPUT%" >nul || goto :failed
del "%OUTPUT%.vbo" >nul 2>nul

REM Test 30: New features audit with gc-stress
"%ROOT%\vesbo.exe" --gc-stress "%~dp0new_features_audit_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"AUDIT_TESTS_PASS" "%OUTPUT%" >nul || goto :failed
"%ROOT%\vesbo.exe" --gc-stress --vm "%~dp0new_features_audit_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"AUDIT_TESTS_PASS" "%OUTPUT%" >nul || goto :failed

REM Test 30: Inline eval flag
"%ROOT%\vesbo.exe" -e "output ""eval_ok""" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"eval_ok" "%OUTPUT%" >nul || goto :failed
"%ROOT%\vesbo.exe" --vm -e "output ""eval_vm_ok""" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"eval_vm_ok" "%OUTPUT%" >nul || goto :failed

REM Test 31: Bug hunt test suite (break in try, VM loop scoping, builtin checks, replace)
"%ROOT%\vesbo.exe" "%~dp0bug_hunt_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"BUG_HUNT_PASS" "%OUTPUT%" >nul || goto :failed

"%ROOT%\vesbo.exe" --vm "%~dp0bug_hunt_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"BUG_HUNT_PASS" "%OUTPUT%" >nul || goto :failed

"%ROOT%\vesbo.exe" --gc-stress "%~dp0bug_hunt_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"BUG_HUNT_PASS" "%OUTPUT%" >nul || goto :failed

"%ROOT%\vesbo.exe" --gc-stress --vm "%~dp0bug_hunt_test.vsb" > "%OUTPUT%" 2>&1
if errorlevel 1 goto :failed
findstr /x /c:"BUG_HUNT_PASS" "%OUTPUT%" >nul || goto :failed

del "%OUTPUT%" >nul 2>nul
echo All Vesbo smoke tests passed.
exit /b 0

:failed
del "%OUTPUT%" >nul 2>nul
echo Vesbo smoke tests failed.
exit /b 1