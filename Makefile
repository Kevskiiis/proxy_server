CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = proxy

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET)