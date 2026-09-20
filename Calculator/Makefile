# Brick Calculator -- deliberately boring Makefile.
# Uses ">" instead of tabs for recipe lines, so copy-pasting can't break it.
.RECIPEPREFIX = >

CC         ?= gcc
PKG_CONFIG ?= pkg-config

TARGET := calculator
SRCS   := main.c
OBJS   := $(SRCS:.c=.o)

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)

ifeq ($(strip $(SDL_LIBS)),)
SDL_LIBS := -lSDL2
endif

CFLAGS  ?= -O2 -pipe
CFLAGS  += -std=gnu99 -Wall -Wextra $(SDL_CFLAGS)
LDLIBS  := $(SDL_LIBS) -lm

DISTDIR ?= ../dist/Calculator

.PHONY: all dist clean

all: $(TARGET)

$(TARGET): $(OBJS)
> $(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c
> $(CC) $(CFLAGS) -c $< -o $@

dist: $(TARGET)
> mkdir -p $(DISTDIR)
> cp $(TARGET) launch.sh config.json $(DISTDIR)/
> chmod 755 $(DISTDIR)/$(TARGET) $(DISTDIR)/launch.sh

clean:
> rm -f $(OBJS) $(TARGET)
