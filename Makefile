CC ?= cc
CFLAGS ?= -Wall -Wextra -Wno-misleading-indentation -O2 -std=gnu2x

.PHONY: all debug clean

all: photosort

photosort: photosort.c
	$(CC) $(CFLAGS) photosort.c -o photosort

debug: photosort.c
	$(CC) -Wall -Wextra -g -O0 -std=gnu2x -fsanitize=address,undefined photosort.c -o photosort

clean:
	rm -rf photosort