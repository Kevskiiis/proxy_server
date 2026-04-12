CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = proxy

all: $(TARGET)

$(TARGET): main.c blocklist.c hosts.c
	$(CC) $(CFLAGS) main.c blocklist.c hosts.c -o $(TARGET)

clean:
	rm -f $(TARGET)