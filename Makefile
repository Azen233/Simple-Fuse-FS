BINS = wfs mkfs
CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18 -g
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`

.PHONY: all
all: $(BINS)

wfs:
	$(CC) $(CFLAGS) src/wfs.c $(FUSE_CFLAGS) -o wfs

mkfs:
	$(CC) $(CFLAGS) -o mkfs src/mkfs.c

.PHONY: clean
clean:
	rm -rf $(BINS)
