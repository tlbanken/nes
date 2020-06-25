# Makefile for nes project
#
# Travis Banken
# 2020

CC = gcc
TARGET = nes

SDIR = src
IDIR = include

CFLAGS = -std=c11
CFLAGS += -I$(IDIR)
CFLAGS += -Wall -Wextra -Wc99-c11-compat
CFLAGS += $(shell sdl2-config --cflags)
# CFLAGS += -O3
CFLAGS += -g
CFLAGS += -DDEBUG

LIBS = $(shell sdl2-config --libs)

SRC = $(wildcard $(SDIR)/*.c)
OBJ = ${SRC:.cpp=.o}
HDRS = $(wildcard $(IDIR)/*.h)

.PHONY: build
build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LIBS) -o $(TARGET)

$(SDIR)/%.o: $(SDIR)/%.c $(HDRS) Makefile
	$(CC) $(CFLAGS) -c $< $(LIBS) -o $@

# cartridges src
$(SDIR)/carts/%.o: $(SDIR)/carts/%.c $(HDRS) Makefile
	$(CC) $(CFLAGS) -c $< $(LIBS) -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJ) *dump* *.log
