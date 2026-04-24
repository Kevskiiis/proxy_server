#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// HEADER FILES:
#include "./req_handler.h"
#include "database/database.h"

#define PORT 3128
#define BUFFER_SIZE 4096
#define CONNECT_TIMEOUT_SEC 10

// Sends a 403 Forbidden response to the client:
static void send_blocked_response(int client_fd, const char *host) {
    char response[512];
    snprintf(response, sizeof(response),
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>403 Forbidden</h1>"
        "<p>Access to <b>%s</b> is blocked by the proxy.</p>"
        "</body></html>",
        host);
    send(client_fd, response, strlen(response), 0);
    printf("[BLOCK] Sent 403 Forbidden for host: %s\n", host);
    fflush(stdout);
}

// Check if a host is blocked including www. variants:
static int is_blocked(const char *host) {
    if (db_site_exists(host)) {
        printf("[BLOCK] Host '%s' is blocked (direct match)\n", host);
        fflush(stdout);
        return 1;
    }

    if (strncmp(host, "www.", 4) == 0) {
        const char *bare = host + 4;
        if (db_site_exists(bare)) {
            printf("[BLOCK] Host '%s' is blocked (bare domain match: %s)\n", host, bare);
            fflush(stdout);
            return 1;
        }
    } else {
        char www_host[256];
        snprintf(www_host, sizeof(www_host), "www.%s", host);
        if (db_site_exists(www_host)) {
            printf("[BLOCK] Host '%s' is blocked (www match: %s)\n", host, www_host);
            fflush(stdout);
            return 1;
        }
    }

    return 0;
}

// Set send and receive timeouts on a socket:
static void set_socket_timeout(int fd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec  = seconds;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

// Handles HTTPS CONNECT tunneling:
void handle_https_tunnel(int client_fd, const char *buffer) {
    printf("[HTTPS] Entered handle_https_tunnel\n");
    fflush(stdout);

    char host[256];
    int port;
    parse_host(buffer, host, &port);
    printf("[HTTPS] Parsed host: '%s', port: %d\n", host, port);
    fflush(stdout);

    if (strlen(host) == 0) {
        printf("[HTTPS] ERROR: host is empty, cannot tunnel\n");
        fflush(stdout);
        close(client_fd);
        return;
    }

    if (is_blocked(host)) {
        printf("[HTTPS] Blocking CONNECT to: %s\n", host);
        fflush(stdout);
        send_blocked_response(client_fd, host);
        close(client_fd);
        return;
    }

    printf("[HTTPS] Creating remote socket...\n");
    fflush(stdout);
    int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        perror("[HTTPS] ERROR: remote socket failed");
        close(client_fd);
        return;
    }
    printf("[HTTPS] Remote socket created (fd=%d)\n", remote_fd);
    fflush(stdout);

    // Set timeout so connect() doesn't hang:
    set_socket_timeout(remote_fd, CONNECT_TIMEOUT_SEC);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(port);

    printf("[HTTPS] Resolving host: '%s'...\n", host);
    fflush(stdout);
    if (resolve_host(host, &dest) < 0) {
        printf("[HTTPS] ERROR: Failed to resolve host '%s'\n", host);
        fflush(stdout);
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTPS] Host resolved successfully\n");
    fflush(stdout);

    printf("[HTTPS] Connecting to remote %s:%d...\n", host, port);
    fflush(stdout);
    if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("[HTTPS] ERROR: tunnel connect failed");
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTPS] Connected to remote %s:%d\n", host, port);
    fflush(stdout);

    char *ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
    int sent = send(client_fd, ok, strlen(ok), 0);
    if (sent < 0) {
        perror("[HTTPS] ERROR: failed to send 200 to client");
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTPS] Sent '200 Connection Established' to client (%d bytes)\n", sent);
    printf("[HTTPS] Tunnel established to %s:%d — entering relay loop\n", host, port);
    fflush(stdout);

    fd_set fds;
    char tunnel_buf[BUFFER_SIZE];
    int bytes;
    int total_client_to_remote = 0;
    int total_remote_to_client = 0;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(remote_fd, &fds);

        int max_fd = remote_fd > client_fd ? remote_fd : client_fd;
        printf("[HTTPS] select() waiting for data...\n");
        fflush(stdout);
        int sel = select(max_fd + 1, &fds, NULL, NULL, NULL);
        if (sel < 0) {
            perror("[HTTPS] ERROR: select failed");
            break;
        }
        printf("[HTTPS] select() returned %d\n", sel);
        fflush(stdout);

        // Client to Remote:
        if (FD_ISSET(client_fd, &fds)) {
            bytes = recv(client_fd, tunnel_buf, sizeof(tunnel_buf), 0);
            if (bytes <= 0) {
                printf("[HTTPS] Client disconnected (recv returned %d)\n", bytes);
                fflush(stdout);
                break;
            }
            total_client_to_remote += bytes;
            printf("[HTTPS] Client → Remote: %d bytes (total: %d)\n", bytes, total_client_to_remote);
            fflush(stdout);
            int s = send(remote_fd, tunnel_buf, bytes, 0);
            if (s < 0) {
                perror("[HTTPS] ERROR: send to remote failed");
                break;
            }
        }

        // Remote to Client:
        if (FD_ISSET(remote_fd, &fds)) {
            bytes = recv(remote_fd, tunnel_buf, sizeof(tunnel_buf), 0);
            if (bytes <= 0) {
                printf("[HTTPS] Remote disconnected (recv returned %d)\n", bytes);
                fflush(stdout);
                break;
            }
            total_remote_to_client += bytes;
            printf("[HTTPS] Remote → Client: %d bytes (total: %d)\n", bytes, total_remote_to_client);
            fflush(stdout);
            int s = send(client_fd, tunnel_buf, bytes, 0);
            if (s < 0) {
                perror("[HTTPS] ERROR: send to client failed");
                break;
            }
        }
    }

    printf("[HTTPS] Tunnel closed for %s:%d | C→R: %d bytes | R→C: %d bytes\n",
           host, port, total_client_to_remote, total_remote_to_client);
    fflush(stdout);
    close(remote_fd);
    close(client_fd);
}

// Handles plain HTTP requests:
void handle_http(int client_fd, char *buffer, int bytes_received) {
    printf("[HTTP] Entered handle_http\n");
    fflush(stdout);

    char host[256];
    int port;
    parse_host(buffer, host, &port);
    printf("[HTTP] Parsed host: '%s', port: %d\n", host, port);
    fflush(stdout);

    if (strlen(host) == 0) {
        printf("[HTTP] ERROR: host is empty, cannot forward\n");
        fflush(stdout);
        close(client_fd);
        return;
    }

    if (is_blocked(host)) {
        printf("[HTTP] Blocking request to: %s\n", host);
        fflush(stdout);
        send_blocked_response(client_fd, host);
        close(client_fd);
        return;
    }

    printf("[HTTP] Creating remote socket...\n");
    fflush(stdout);
    int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        perror("[HTTP] ERROR: remote socket failed");
        close(client_fd);
        return;
    }
    printf("[HTTP] Remote socket created (fd=%d)\n", remote_fd);
    fflush(stdout);

    // Set timeout so connect() doesn't hang:
    set_socket_timeout(remote_fd, CONNECT_TIMEOUT_SEC);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(port);

    printf("[HTTP] Resolving host: '%s'...\n", host);
    fflush(stdout);
    if (resolve_host(host, &dest) < 0) {
        printf("[HTTP] ERROR: Failed to resolve host '%s'\n", host);
        fflush(stdout);
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTP] Host resolved successfully\n");
    fflush(stdout);

    printf("[HTTP] Connecting to remote %s:%d...\n", host, port);
    fflush(stdout);
    if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("[HTTP] ERROR: connect failed");
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTP] Connected to remote %s:%d\n", host, port);
    fflush(stdout);

    printf("[HTTP] Forwarding %d bytes to remote...\n", bytes_received);
    fflush(stdout);
    if (send(remote_fd, buffer, bytes_received, 0) < 0) {
        perror("[HTTP] ERROR: send to remote failed");
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTP] Request forwarded successfully\n");
    fflush(stdout);

    char response[BUFFER_SIZE];
    int response_bytes;
    int total = 0;
    printf("[HTTP] Waiting for response from remote...\n");
    fflush(stdout);
    while ((response_bytes = recv(remote_fd, response, sizeof(response), 0)) > 0) {
        total += response_bytes;
        printf("[HTTP] Received %d bytes from remote (total: %d), forwarding to client...\n",
               response_bytes, total);
        fflush(stdout);
        int s = send(client_fd, response, response_bytes, 0);
        if (s < 0) {
            perror("[HTTP] ERROR: send to client failed");
            break;
        }
    }
    printf("[HTTP] Done. Total response: %d bytes\n", total);
    fflush(stdout);

    close(remote_fd);
    close(client_fd);
    printf("[HTTP] Sockets closed\n");
    fflush(stdout);
}

int main() {
    int server_fd;
    struct sockaddr_in address;

    // Initialize DB on startup:
    if (!db_init()) {
        fprintf(stderr, "[MAIN] Failed to initialize database.\n");
        return 1;
    }

    printf("[MAIN] Starting proxy...\n");
    fflush(stdout);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[MAIN] ERROR: socket failed");
        return 1;
    }
    printf("[MAIN] Server socket created (fd=%d)\n", server_fd);
    fflush(stdout);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    printf("[MAIN] SO_REUSEADDR set\n");
    fflush(stdout);

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("[MAIN] ERROR: bind failed");
        return 1;
    }
    printf("[MAIN] Bound to port %d\n", PORT);
    fflush(stdout);

    if (listen(server_fd, 10) < 0) {
        perror("[MAIN] ERROR: listen failed");
        return 1;
    }

    printf("[MAIN] Proxy listening on port %d...\n", PORT);
    fflush(stdout);

    while (1) {
        printf("[MAIN] Waiting for connection...\n");
        fflush(stdout);

        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("[MAIN] ERROR: accept failed");
            continue;
        }

        // Set timeout on client socket so recv() doesn't hang:
        set_socket_timeout(client_fd, CONNECT_TIMEOUT_SEC);

        printf("[MAIN] Accepted connection (client_fd=%d)\n", client_fd);
        fflush(stdout);

        pid_t pid = fork();
        if (pid < 0) {
            perror("[MAIN] ERROR: fork failed");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // Child process handles this connection:
            printf("[CHILD pid=%d] Handling new connection\n", getpid());
            fflush(stdout);
            close(server_fd);

            char buffer[BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));
            printf("[CHILD pid=%d] Waiting to recv from client...\n", getpid());
            fflush(stdout);
            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                printf("[CHILD pid=%d] ERROR: recv returned %d, closing\n", getpid(), bytes_received);
                fflush(stdout);
                close(client_fd);
                exit(0);
            }
            printf("[CHILD pid=%d] Received %d bytes from client\n", getpid(), bytes_received);
            fflush(stdout);
            printf("[CHILD pid=%d] Request:\n%s\n", getpid(), buffer);
            fflush(stdout);

            if (strncmp(buffer, "CONNECT", 7) == 0) {
                printf("[CHILD pid=%d] Detected HTTPS CONNECT request\n", getpid());
                fflush(stdout);
                handle_https_tunnel(client_fd, buffer);
            } else {
                printf("[CHILD pid=%d] Detected plain HTTP request\n", getpid());
                fflush(stdout);
                handle_http(client_fd, buffer, bytes_received);
            }

            printf("[CHILD pid=%d] Done, exiting\n", getpid());
            fflush(stdout);
            exit(0);
        }

        // Parent closes its copy and loops back to accept:
        printf("[MAIN] Forked child pid=%d, closing parent's client_fd\n", pid);
        fflush(stdout);
        close(client_fd);

        // Reap finished child processes to avoid zombies:
        waitpid(-1, NULL, WNOHANG);
    }

    close(server_fd);
    return 0;
}