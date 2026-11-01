CC ?= gcc
CFLAGS ?= -Wall -Wextra
CFLAGS += -Iinclude
LDLIBS ?=
LDLIBS += -lm

SRC = src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c \
      src/builtins.c src/module.c src/interpreter.c \
      src/bytecode.c src/codegen.c src/vm.c src/disassemble.c src/repl.c src/main.c

ifeq ($(OS),Windows_NT)
    TARGET ?= vesbo.exe
else
    TARGET ?= vesbo
endif

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

test: $(TARGET)
	./tests/run_tests.sh

clean:
	rm -f $(TARGET) $(TARGET).exe *.vbo tests/*.vbo

.PHONY: clean test
