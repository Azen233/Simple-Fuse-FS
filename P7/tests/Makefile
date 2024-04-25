SHELL := /bin/bash
# Define the C compiler
CC=gcc
# Define any compile-time flags
CFLAGS=-Wall -g
# Define the source files
SOURCES:=$(shell echo {2..19}.c)
# Define the binaries to create (with the same name as the source file but no extension)
OBJECTS:=$(SOURCES:.c=.o)
BINARIES:=$(SOURCES:.c=) mkfs_check

$(info $(BINARIES))

.PHONY: all clean

all: $(BINARIES)

%: %.c common/utils.c
	$(info Building $@)
	$(CC) $(CFLAGS) $^ -o $@


# Rule to clean binaries
clean:
	rm -f *.o $(BINARIES)
