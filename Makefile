CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS =

# Use pkg-config to get the right GTK flags
PKG_CONFIG ?= pkg-config
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0)
GTK_LIBS   := $(shell $(PKG_CONFIG) --libs gtk+-3.0)

TARGETS = server client

all: $(TARGETS)

# ---- server (no GTK) ----
server: server.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c communication.h
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- client (needs GTK) ----
client: client.o
	$(CC) $(CFLAGS) -o $@ $^ $(GTK_LIBS) $(LDFLAGS)

client.o: client.c communication.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
