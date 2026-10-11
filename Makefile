CC ?= gcc
AR ?= ar
CFLAGS ?= -Wall -Wextra
CFLAGS += -Iinclude
LDLIBS ?=
LDLIBS += -lm

LIB_SRC = src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c \
          src/builtins.c src/raylib_bindings.c src/module.c src/interpreter.c \
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

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

ifeq ($(NO_RAYLIB),1)
    CFLAGS += -DNO_RAYLIB
else
    ifeq ($(shell pkg-config --exists raylib 2>/dev/null && echo 1),1)
        CFLAGS += $(shell pkg-config --cflags raylib)
        LDLIBS += $(shell pkg-config --libs raylib)
        ifeq ($(UNAME_S),Darwin)
            LDLIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
        endif
    else
        ifeq ($(UNAME_S),Darwin)
            ifeq ($(shell test -f /opt/homebrew/include/raylib.h && echo 1),1)
                CFLAGS += -I/opt/homebrew/include
                LDLIBS += -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
            else ifeq ($(shell test -f /usr/local/include/raylib.h && echo 1),1)
                CFLAGS += -I/usr/local/include
                LDLIBS += -L/usr/local/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
            else
                CFLAGS += -DNO_RAYLIB
            endif
        else ifeq ($(OS),Windows_NT)
            ifeq ($(shell if exist vendor\raylib\include\raylib.h (echo 1) else (echo 0)),1)
                CFLAGS += -Ivendor/raylib/include
                LDLIBS += -Lvendor/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm
            else
                CFLAGS += -DNO_RAYLIB
            endif
        else
            ifeq ($(shell test -f /usr/include/raylib.h && echo 1),1)
                LDLIBS += -lraylib -lGL -lpthread -ldl -lrt -lX11
            else ifeq ($(shell test -f /usr/local/include/raylib.h && echo 1),1)
                CFLAGS += -I/usr/local/include
                LDLIBS += -L/usr/local/lib -lraylib -lGL -lpthread -ldl -lrt -lX11
            else
                CFLAGS += -DNO_RAYLIB
            endif
        endif
    endif
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
	rm -f $(TARGET) $(TARGET).exe $(EMBED_TEST) tests/embed_host_test tests/embed_host_test.exe $(LIB_TARGET) libvesbo.lib $(LIB_OBJ) *.vbo examples/*.vbo tests/*.vbo games/breakout/*.vbo

.PHONY: all clean test
