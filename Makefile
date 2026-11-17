CC ?= gcc
PKG_CONFIG ?= pkg-config

CFLAGS = -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags gtk4 libserialport)
LDFLAGS = $(shell $(PKG_CONFIG) --libs gtk4 libserialport) -lm

SRCS = acom2000.c lib.c
OBJS = $(SRCS:.c=.o)
TARGET = acom2000

# Detect Windows (MSYS2 / MinGW / Cygwin)
ifeq ($(OS),Windows_NT)
	TARGET := $(TARGET).exe
	LDFLAGS += -mwindows
endif

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c extern.h config.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) *~

# Optional install rule for Linux/macOS
ifneq ($(OS),Windows_NT)
PREFIX ?= /usr/local
install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
endif
