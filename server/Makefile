CC = gcc
CFLAGS = -Wall -Wextra -g
TARGETS = proxy server

all: $(TARGETS)

proxy: proxy.c blocklist.c req_handler.h
	$(CC) $(CFLAGS) proxy.c blocklist.c -o proxy

server: main.c blocklist.c hosts.c
	$(CC) $(CFLAGS) main.c blocklist.c hosts.c -o server

clean:
	rm -f $(TARGETS)