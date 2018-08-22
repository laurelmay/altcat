CC=gcc

override CFLAGS := -Wall -pedantic -std=c99 -O2 $(CFLAGS)
override LDFLAGS := $(LDFLAGS)

EXE=altcat

all: $(EXE)

$(EXE): $(EXE).c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -rf altcat

.PHONY: clean
