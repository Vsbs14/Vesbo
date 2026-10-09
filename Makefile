CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
LDFLAGS = -lm
SRC = src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c src/builtins.c src/module.c src/interpreter.c src/bytecode.c src/codegen.c src/vm.c src/disassemble.c src/repl.c src/main.c
TARGET = vesbo

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.vbo

.PHONY: clean