CC ?= gcc
CFLAGS ?= -Wall -Wextra
CFLAGS += -Iinclude
LDLIBS ?=
LDLIBS += -lm

SRC = src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c \
      src/builtins.c src/raylib_bindings.c src/module.c src/interpreter.c \
      src/bytecode.c src/codegen.c src/vm.c src/disassemble.c src/repl.c src/main.c

ifeq ($(OS),Windows_NT)
    TARGET ?= vesbo.exe
else
    TARGET ?= vesbo
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

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

clean:
	rm -f $(TARGET) $(TARGET).exe *.vbo tests/*.vbo

.PHONY: clean
