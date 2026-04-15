#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// HEADER FILES:
#include "./req_handler.h"

#define PORT 3128
#define BUFFER_SIZE 4096

// Handles HTTPS CONNECT tunneling:
void handle_https_tunnel(int client_fd, const char *buffer) {

    // Parse the host and port from the CONNECT request
    char host[256];
    int port;
    parse_host(buffer, host, &port);
    printf("HTTPS Tunnel to: %s on port: %d\n", host, port);

    // Create a fresh remote socket:
    int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        perror("remote socket failed");
        close(client_fd);
        return;
    }

    // Setup the destination address:
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(port);

    // Resolve the hostname to an IP:
    if (resolve_host(host, &dest) < 0) {
        close(remote_fd);
        close(client_fd);
        return;
    }

    // Connect to the remote server:
    if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("tunnel connect failed");
        close(remote_fd);
        close(client_fd);
        return;
    }

    // Tell the client the tunnel is ready:
    char *ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client_fd, ok, strlen(ok), 0);
    printf("Tunnel established to %s:%d\n", host, port);

    // Blindly forward bytes in both directions:
    fd_set fds;
    char tunnel_buf[BUFFER_SIZE];
    int bytes;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(remote_fd, &fds);

        // Wait for data on either socket
        int max_fd = remote_fd > client_fd ? remote_fd : client_fd;
        if (select(max_fd + 1, &fds, NULL, NULL, NULL) < 0) {
            perror("select failed");
            break;
        }

        // Client → Remote
        if (FD_ISSET(client_fd, &fds)) {
            bytes = recv(client_fd, tunnel_buf, sizeof(tunnel_buf), 0);
            if (bytes <= 0) break; // Client disconnected
            send(remote_fd, tunnel_buf, bytes, 0);
        }

        // Remote → Client
        if (FD_ISSET(remote_fd, &fds)) {
            bytes = recv(remote_fd, tunnel_buf, sizeof(tunnel_buf), 0);
            if (bytes <= 0) break; // Remote disconnected
            send(client_fd, tunnel_buf, bytes, 0);
        }
    }

    printf("Tunnel closed for %s:%d\n", host, port);
    close(remote_fd);
    close(client_fd);
}

// Handles plain HTTP requests:
void handle_http(int client_fd, char *buffer, int bytes_received) {

    // Parse the host and port from the HTTP request:
    char host[256];
    int port;
    parse_host(buffer, host, &port);
    printf("Connecting to host: %s on port: %d\n", host, port);

    // Create a fresh remote socket:
    int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        perror("remote socket failed");
        close(client_fd);
        return;
    }

    // Setup the destination address:
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(port);

    // Resolve the hostname to an IP:
    if (resolve_host(host, &dest) < 0) {
        close(remote_fd);
        close(client_fd);
        return;
    }

    // Connect to the remote server:
    if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("connect failed");
        close(remote_fd);
        close(client_fd);
        return;
    }

    // Forward the HTTP request to the remote server:
    if (send(remote_fd, buffer, bytes_received, 0) < 0) {
        perror("send failed");
        close(remote_fd);
        close(client_fd);
        return;
    }

    // Receive the response and forward back to client:
    char response[BUFFER_SIZE];
    int response_bytes;
    while ((response_bytes = recv(remote_fd, response, sizeof(response), 0)) > 0) {
        send(client_fd, response, response_bytes, 0);
    }

    close(remote_fd);
    close(client_fd);
}

int main() {
    int server_fd;
    struct sockaddr_in address;

    // Create the "client <=> proxy" server socket: ==========================
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    // Allow port reuse so we can restart the proxy quickly
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    // Listen for connections:
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        return 1;
    }

    printf("Proxy listening on port %d...\n", PORT);

    while (1) {
        // Accept incoming client connections:
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        // Read the HTTP request from the client:
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            perror("recv failed");
            close(client_fd);
            continue;
        }

        printf("Received request:\n%s\n", buffer);

        // Detect if it's HTTPS (CONNECT) or plain HTTP:
        if (strncmp(buffer, "CONNECT", 7) == 0) {
            handle_https_tunnel(client_fd, buffer);  // HTTPS tunnel
        } else {
            handle_http(client_fd, buffer, bytes_received); // Plain HTTP
        }
    }

    close(server_fd);
    return 0;
}