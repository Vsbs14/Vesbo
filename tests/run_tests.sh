#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT="$(mktemp "/tmp/vesbo-tests-XXXXXX.txt")"
VBO_FILE="${OUTPUT}.vbo"

cleanup() {
    rm -f "$OUTPUT" "$VBO_FILE" "$ROOT/test_io_temp.txt" "test_io_temp.txt"
}
trap cleanup EXIT

failed() {
    echo "Vesbo smoke tests failed." >&2
    exit 1
}

# Ensure binary is built
if [ ! -x "$ROOT/vesbo" ]; then
    make -C "$ROOT" || failed
fi

cd "$ROOT"

# Test 1: runtime regressions
./vesbo tests/runtime_regressions.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "7" "$OUTPUT" || failed
grep -Fxq "20" "$OUTPUT" || failed
grep -Fxq "number(): expected a valid number" "$OUTPUT" || failed
grep -Fxq "array index must be an integer" "$OUTPUT" || failed

# Test 2: array loops example
./vesbo examples/array_loops.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "108" "$OUTPUT" || failed
grep -Fxq "18" "$OUTPUT" || failed

# Test 3: try/catch example
./vesbo examples/try_catch_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "before error" "$OUTPUT" || failed
grep -Fxq "division by zero" "$OUTPUT" || failed
grep -Fxq "after try/catch" "$OUTPUT" || failed

# Test 4: All features in tree-walking interpreter
./vesbo tests/all_features_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "apple - banana - orange" "$OUTPUT" || failed
grep -Fxq "bonono" "$OUTPUT" || failed
grep -Fxq "Handled: division by zero" "$OUTPUT" || failed

# Test 5: All features in bytecode VM
./vesbo -c tests/all_features_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "apple - banana - orange" "$OUTPUT" || failed
grep -Fxq "bonono" "$OUTPUT" || failed
grep -Fxq "Handled: division by zero" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 6: New syntax in interpreter
./vesbo tests/new_syntax_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "hello world" "$OUTPUT" || failed
grep -Fxq "hello single" "$OUTPUT" || failed
grep -Fxq "hash # string" "$OUTPUT" || failed
grep -Fxq "Hello, Alice!" "$OUTPUT" || failed
grep -Fxq "15" "$OUTPUT" || failed

# Test 7: New syntax in bytecode VM
./vesbo -c tests/new_syntax_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "hello world" "$OUTPUT" || failed
grep -Fxq "hello single" "$OUTPUT" || failed
grep -Fxq "hash # string" "$OUTPUT" || failed
grep -Fxq "Hello, Alice!" "$OUTPUT" || failed
grep -Fxq "15" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 8: Maps and index mutation in interpreter
./vesbo tests/map_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "100" "$OUTPUT" || failed
grep -Fxq "999" "$OUTPUT" || failed
grep -Fxq "25" "$OUTPUT" || failed

# Test 9: Maps and index mutation in bytecode VM
./vesbo -c tests/map_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "100" "$OUTPUT" || failed
grep -Fxq "999" "$OUTPUT" || failed
grep -Fxq "25" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 10: File and OS built-ins in interpreter
./vesbo tests/io_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "Hello Vesbo I/O! Extra content." "$OUTPUT" || failed
grep -Fxq "none" "$OUTPUT" || failed

# Test 11: File and OS built-ins in bytecode VM
./vesbo -c tests/io_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "Hello Vesbo I/O! Extra content." "$OUTPUT" || failed
grep -Fxq "none" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 12: Modules and imports in interpreter
./vesbo tests/import_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "import test success" "$OUTPUT" || failed

# Test 13: Modules and imports in bytecode VM
./vesbo -c tests/import_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "import test success" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 14: Circular imports in interpreter
./vesbo tests/circ_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "circular import success" "$OUTPUT" || failed

# Test 15: Circular imports in bytecode VM
./vesbo -c tests/circ_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "circular import success" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 16: String interpolation in interpreter
./vesbo tests/interpolation_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "interpolation test success" "$OUTPUT" || failed
grep -Fxq "Hello, World!" "$OUTPUT" || failed
grep -Fxq "User: Alice, age: 30" "$OUTPUT" || failed

# Test 17: String interpolation in bytecode VM
./vesbo -c tests/interpolation_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "interpolation test success" "$OUTPUT" || failed
grep -Fxq "Hello, World!" "$OUTPUT" || failed
grep -Fxq "User: Alice, age: 30" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 18: Standard Library in interpreter
./vesbo tests/stdlib_test.vsb > "$OUTPUT" 2>&1 || failed
grep -Fxq "stdlib test success" "$OUTPUT" || failed
grep -Fxq "reversed: 20, 15, 10, 5" "$OUTPUT" || failed
grep -Fxq "pad_left: 007" "$OUTPUT" || failed

# Test 19: Standard Library in bytecode VM
./vesbo -c tests/stdlib_test.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fxq "stdlib test success" "$OUTPUT" || failed
grep -Fxq "reversed: 20, 15, 10, 5" "$OUTPUT" || failed
grep -Fxq "pad_left: 007" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 20: Disassembler on .vsb source
./vesbo -d examples/array_loops.vsb > "$OUTPUT" 2>&1 || failed
grep -Fq "Bytecode Disassembly" "$OUTPUT" || failed
grep -Fq "OP_PUSH_ARRAY" "$OUTPUT" || failed
grep -Fq "OP_HALT" "$OUTPUT" || failed

# Test 21: Disassembler on .vbo bytecode
./vesbo -c examples/array_loops.vsb -o "$VBO_FILE" > /dev/null 2>&1 || failed
./vesbo -d "$VBO_FILE" > "$OUTPUT" 2>&1 || failed
grep -Fq "Bytecode Disassembly" "$OUTPUT" || failed
grep -Fq "OP_PUSH_ARRAY" "$OUTPUT" || failed
rm -f "$VBO_FILE"

# Test 22: REPL expression evaluation via pipe
echo "21 * 2" | ./vesbo > "$OUTPUT" 2>&1 || failed
grep -Fxq "42" "$OUTPUT" || failed

# Test 23: CLI flags
./vesbo --version > "$OUTPUT" 2>&1 || failed
grep -Fq "Vesbo 0.2.0" "$OUTPUT" || failed

./vesbo --help > "$OUTPUT" 2>&1 || failed
grep -Fq "Usage:" "$OUTPUT" || failed
grep -Fq -e "--disassemble" "$OUTPUT" || failed
grep -Fq -e "--repl" "$OUTPUT" || failed

echo "All Vesbo smoke tests passed."
exit 0
