# ============================
#   MATRIX GAME — Makefile
# ============================

# Compiler
CC     = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude

# SDL2 libraries
LIBS = -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer

# Source files
SRC  = $(wildcard src/*.c)
OBJ  = $(SRC:.c=.o)

# Output executable
TARGET = game

# ==============================
#   Build rules
# ==============================

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ==============================
#   Utility commands
# ==============================

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f src/*.o $(TARGET)

rebuild: clean all