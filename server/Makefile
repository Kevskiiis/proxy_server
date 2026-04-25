# ---------------------------------------------------
CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lsqlite3
TARGETS = proxy server

all: $(TARGETS)

proxy: proxy.c database/database.c req_handler.h
	$(CC) $(CFLAGS) proxy.c database/database.c -o proxy $(LIBS)

server: main.c database/database.c hosts.c
	$(CC) $(CFLAGS) main.c database/database.c hosts.c -o server $(LIBS)

clean:
	rm -f $(TARGETS)