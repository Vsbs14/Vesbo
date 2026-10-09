CC ?= gcc
CFLAGS ?= -Wall -Wextra
CFLAGS += -Iinclude
LDFLAGS ?= -lm

SRC = src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c \
      src/builtins.c src/raylib_bindings.c src/module.c src/interpreter.c \
      src/bytecode.c src/codegen.c src/vm.c src/disassemble.c src/repl.c src/main.c
TARGET = vesbo

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

ifeq ($(NO_RAYLIB),1)
    CFLAGS += -DNO_RAYLIB
else
    ifeq ($(shell pkg-config --exists raylib 2>/dev/null && echo 1),1)
        CFLAGS += $(shell pkg-config --cflags raylib)
        LDFLAGS += $(shell pkg-config --libs raylib)
    else
        ifeq ($(UNAME_S),Darwin)
            ifeq ($(shell test -f /opt/homebrew/include/raylib.h && echo 1),1)
                CFLAGS += -I/opt/homebrew/include
                LDFLAGS += -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
            else ifeq ($(shell test -f /usr/local/include/raylib.h && echo 1),1)
                CFLAGS += -I/usr/local/include
                LDFLAGS += -L/usr/local/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
            else
                CFLAGS += -DNO_RAYLIB
            endif
        else
            ifeq ($(shell test -f /usr/include/raylib.h && echo 1),1)
                LDFLAGS += -lraylib -lGL -lpthread -ldl -lrt -lX11
            else ifeq ($(shell test -f /usr/local/include/raylib.h && echo 1),1)
                CFLAGS += -I/usr/local/include
                LDFLAGS += -L/usr/local/lib -lraylib -lGL -lpthread -ldl -lrt -lX11
            else
                CFLAGS += -DNO_RAYLIB
            endif
        endif
    endif
endif

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET).exe *.vbo tests/*.vbo

.PHONY: clean
