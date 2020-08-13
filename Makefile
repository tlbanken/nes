# Makefile for nes project
#
# Travis Banken
# 2020

CC = gcc
TARGET = nes

SDIR = src
IDIR = include
MDIR = src/mappers

CFLAGS = -std=c11
CFLAGS += -I$(IDIR)
CFLAGS += -Wall -Wextra -Wc99-c11-compat
# all the warnings!
CFLAGS += -Wshadow -Wdouble-promotion -Wundef -Wduplicated-cond \
		  -Wduplicated-branches -Wlogical-op -Wnull-dereference -Wformat=2
CFLAGS += $(shell sdl2-config --cflags)
# CFLAGS += -O3 -s -DNDEBUG # Release Mode
CFLAGS += -g
# CFLAGS += -pg # for profiling
CFLAGS += -DDEBUG

LIBS = $(shell sdl2-config --libs)

SRC = $(wildcard $(SDIR)/*.c) $(wildcard $(MDIR)/*.c)
OBJ = ${SRC:.c=.o}
HDRS = $(wildcard $(IDIR)/*.h)

.PHONY: build
build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LIBS) -o $(TARGET)

$(SDIR)/%.o: $(SDIR)/%.c $(HDRS) Makefile
	$(CC) $(CFLAGS) -c $< $(LIBS) -o $@

$(MDIR)/%.o: $(MDIR)/%.c $(HDRS) Makefile
	$(CC) $(CFLAGS) -c $< $(LIBS) -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJ) *.dump *.log
