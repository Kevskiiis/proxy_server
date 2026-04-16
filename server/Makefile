CC = gcc
CFLAGS = -Wall -Wextra -g
BIN_DIR = bin
API_TARGET = $(BIN_DIR)/cs_guardian_c_api
PROXY_TARGET = $(BIN_DIR)/cs_guardian_c_proxy

all: $(API_TARGET) $(PROXY_TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(API_TARGET): main.c blocklist.c hosts.c | $(BIN_DIR)
	$(CC) $(CFLAGS) main.c blocklist.c hosts.c -o $(API_TARGET)

$(PROXY_TARGET): proxy.c | $(BIN_DIR)
	$(CC) $(CFLAGS) proxy.c -o $(PROXY_TARGET)

clean:
	rm -f $(API_TARGET) $(PROXY_TARGET)
