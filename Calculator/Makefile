# Build the TrimUI Brick calculator.
#
# On the device (inside the union-tg5040-toolchain shell — see README.md):
#   make
#
# On your PC, for a quick sanity-check with your desktop SDL2 (arrow keys /
# Z / X / C / V / A|S to test the controls before deploying):
#   make host
#   ./calculator-host

CC ?= gcc
SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null || sdl2-config --cflags)
SDL2_LIBS   := $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null || sdl2-config --libs) -lm

SRC := src/main.c
BIN := calculator
HOSTBIN := calculator-host

.PHONY: all host clean

all: $(BIN)

$(BIN): $(SRC)
	$(CC) -O2 -Wall $(SDL2_CFLAGS) $(SRC) -o $(BIN) $(SDL2_LIBS)

host: $(SRC)
	$(CC) -O2 -Wall $(SDL2_CFLAGS) $(SRC) -o $(HOSTBIN) $(SDL2_LIBS)

clean:
	rm -f $(BIN) $(HOSTBIN)
