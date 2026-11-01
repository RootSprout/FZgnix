CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
SOURCES := $(shell find src -name '*.c' -print)

.PHONY: all clean run

all: ngnix

ngnix: $(SOURCES)
	$(CC) $(CFLAGS) -Isrc -o $@ $(SOURCES)

run: ngnix
	./ngnix -c ngnix.conf

clean:
	rm -f ngnix
