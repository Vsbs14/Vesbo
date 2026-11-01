CC ?= gcc
AR ?= ar
CFLAGS ?= -Wall -Wextra
CFLAGS += -Iinclude
LDLIBS ?=
LDLIBS += -lm

LIB_SRC = src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c \
          src/builtins.c src/module.c src/interpreter.c \
          src/bytecode.c src/codegen.c src/vm.c src/disassemble.c src/repl.c src/vesbo.c
LIB_OBJ = $(LIB_SRC:.c=.o)
CLI_SRC = src/main.c
LIB_TARGET = libvesbo.a

ifeq ($(OS),Windows_NT)
    TARGET ?= vesbo.exe
    EMBED_TEST ?= tests/embed_host_test.exe
else
    TARGET ?= vesbo
    EMBED_TEST ?= tests/embed_host_test
endif

all: $(LIB_TARGET) $(TARGET) $(EMBED_TEST)

$(LIB_TARGET): $(LIB_OBJ)
	$(AR) rcs $(LIB_TARGET) $(LIB_OBJ)

$(TARGET): $(CLI_SRC) $(LIB_TARGET)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(CLI_SRC) $(LIB_TARGET) $(LDLIBS)

$(EMBED_TEST): tests/embed_host_test.c $(LIB_TARGET)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(EMBED_TEST) tests/embed_host_test.c $(LIB_TARGET) $(LDLIBS)

test: $(TARGET) $(EMBED_TEST)
	./tests/run_tests.sh

clean:
	rm -f $(TARGET) $(TARGET).exe $(EMBED_TEST) tests/embed_host_test tests/embed_host_test.exe $(LIB_TARGET) libvesbo.lib $(LIB_OBJ) *.vbo examples/*.vbo tests/*.vbo

.PHONY: all clean test
