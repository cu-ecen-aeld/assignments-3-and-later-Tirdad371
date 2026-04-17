# Makefile for aesdsocket
#
# Native build:
#   make
#
# Cross-compiled build (example for ARM):
#   make CROSS_COMPILE=aarch64-linux-gnu-
#
# Clean:
#   make clean

CROSS_COMPILE ?=
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -Wall -Werror -g
TARGET  = aesdsocket
SRC     = aesdsocket.c

.PHONY: all default clean

all default: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
