CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC = src/lexer.c src/ast.c src/parser.c src/value.c src/environment.c src/builtins.c src/interpreter.c src/main.c
TARGET = vesbo

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: clean
