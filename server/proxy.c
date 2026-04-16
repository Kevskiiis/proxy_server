// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <sys/select.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <netdb.h>

// // HEADER FILES:
// #include "./req_handler.h"

// #define PORT 3128
// #define BUFFER_SIZE 4096

// // Handles HTTPS CONNECT tunneling:
// void handle_https_tunnel(int client_fd, const char *buffer) {

//     // Parse the host and port from the CONNECT request
//     char host[256];
//     int port;
//     parse_host(buffer, host, &port);
//     printf("HTTPS Tunnel to: %s on port: %d\n", host, port);

//     // Create a fresh remote socket:
//     int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (remote_fd < 0) {
//         perror("remote socket failed");
//         close(client_fd);
//         return;
//     }

//     // Setup the destination address:
//     struct sockaddr_in dest;
//     memset(&dest, 0, sizeof(dest));
//     dest.sin_family = AF_INET;
//     dest.sin_port   = htons(port);

//     // Resolve the hostname to an IP:
//     if (resolve_host(host, &dest) < 0) {
//         close(remote_fd);
//         close(client_fd);
//         return;
//     }

//     // Connect to the remote server:
//     if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
//         perror("tunnel connect failed");
//         close(remote_fd);
//         close(client_fd);
//         return;
//     }

//     // Tell the client the tunnel is ready:
//     char *ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
//     send(client_fd, ok, strlen(ok), 0);
//     printf("Tunnel established to %s:%d\n", host, port);

//     // Blindly forward bytes in both directions:
//     fd_set fds;
//     char tunnel_buf[BUFFER_SIZE];
//     int bytes;

//     while (1) {
//         FD_ZERO(&fds);
//         FD_SET(client_fd, &fds);
//         FD_SET(remote_fd, &fds);

//         // Wait for data on either socket
//         int max_fd = remote_fd > client_fd ? remote_fd : client_fd;
//         if (select(max_fd + 1, &fds, NULL, NULL, NULL) < 0) {
//             perror("select failed");
//             break;
//         }

//         // Client → Remote
//         if (FD_ISSET(client_fd, &fds)) {
//             bytes = recv(client_fd, tunnel_buf, sizeof(tunnel_buf), 0);
//             if (bytes <= 0) break; // Client disconnected
//             send(remote_fd, tunnel_buf, bytes, 0);
//         }

//         // Remote → Client
//         if (FD_ISSET(remote_fd, &fds)) {
//             bytes = recv(remote_fd, tunnel_buf, sizeof(tunnel_buf), 0);
//             if (bytes <= 0) break; // Remote disconnected
//             send(client_fd, tunnel_buf, bytes, 0);
//         }
//     }

//     printf("Tunnel closed for %s:%d\n", host, port);
//     close(remote_fd);
//     close(client_fd);
// }

// // Handles plain HTTP requests:
// void handle_http(int client_fd, char *buffer, int bytes_received) {

//     // Parse the host and port from the HTTP request:
//     char host[256];
//     int port;
//     parse_host(buffer, host, &port);
//     printf("Connecting to host: %s on port: %d\n", host, port);

//     // Create a fresh remote socket:
//     int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (remote_fd < 0) {
//         perror("remote socket failed");
//         close(client_fd);
//         return;
//     }

//     // Setup the destination address:
//     struct sockaddr_in dest;
//     memset(&dest, 0, sizeof(dest));
//     dest.sin_family = AF_INET;
//     dest.sin_port   = htons(port);

//     // Resolve the hostname to an IP:
//     if (resolve_host(host, &dest) < 0) {
//         close(remote_fd);
//         close(client_fd);
//         return;
//     }

//     // Connect to the remote server:
//     if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
//         perror("connect failed");
//         close(remote_fd);
//         close(client_fd);
//         return;
//     }

//     // Forward the HTTP request to the remote server:
//     if (send(remote_fd, buffer, bytes_received, 0) < 0) {
//         perror("send failed");
//         close(remote_fd);
//         close(client_fd);
//         return;
//     }

//     // Receive the response and forward back to client:
//     char response[BUFFER_SIZE];
//     int response_bytes;
//     while ((response_bytes = recv(remote_fd, response, sizeof(response), 0)) > 0) {
//         send(client_fd, response, response_bytes, 0);
//     }

//     close(remote_fd);
//     close(client_fd);
// }

// int main() {
//     int server_fd;
//     struct sockaddr_in address;

//     // Create the "client <=> proxy" server socket: ==========================
//     server_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (server_fd < 0) {
//         perror("socket failed");
//         return 1;
//     }

//     // Allow port reuse so we can restart the proxy quickly
//     int opt = 1;
//     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//     address.sin_family      = AF_INET;
//     address.sin_addr.s_addr = INADDR_ANY;
//     address.sin_port        = htons(PORT);

//     if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
//         perror("bind failed");
//         return 1;
//     }

//     // Listen for connections:
//     if (listen(server_fd, 10) < 0) {
//         perror("listen failed");
//         return 1;
//     }

//     printf("Proxy listening on port %d...\n", PORT);

//     while (1) {
//         // Accept incoming client connections:
//         int client_fd = accept(server_fd, NULL, NULL);
//         if (client_fd < 0) {
//             perror("accept failed");
//             continue;
//         }

//         pid_t pid = fork();
//         if (pid == 0) {
//             // Child process handles this connection:
//             close(server_fd);

//             // Read the HTTP request from the client:
//             char buffer[BUFFER_SIZE];
//             memset(buffer, 0, sizeof(buffer));
//             int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
//             if (bytes_received <= 0) {
//                 perror("recv failed");
//                 close(client_fd);
//                 exit(0);
//             }

//             printf("Received request:\n%s\n", buffer);

//             // Detect if it's HTTPS (CONNECT) or plain HTTP:
//             if (strncmp(buffer, "CONNECT", 7) == 0) {
//                 handle_https_tunnel(client_fd, buffer);  // HTTPS tunnel
//             } else {
//                 handle_http(client_fd, buffer, bytes_received); // Plain HTTP
//             }

//             exit(0);
//         }

//         // Parent closes its copy and loops back to accept:
//         close(client_fd);
//     }

//     close(server_fd);
//     return 0;
// }


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

#define PORT 3128
#define BUFFER_SIZE 4096

// Handles HTTPS CONNECT tunneling:
void handle_https_tunnel(int client_fd, const char *buffer) {

    printf("[HTTPS] Entered handle_https_tunnel\n");

    // Parse the host and port from the CONNECT request
    char host[256];
    int port;
    parse_host(buffer, host, &port);
    printf("[HTTPS] Parsed host: '%s', port: %d\n", host, port);

    if (strlen(host) == 0) {
        printf("[HTTPS] ERROR: host is empty, cannot tunnel\n");
        close(client_fd);
        return;
    }

    // Create a fresh remote socket:
    printf("[HTTPS] Creating remote socket...\n");
    int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        perror("[HTTPS] ERROR: remote socket failed");
        close(client_fd);
        return;
    }
    printf("[HTTPS] Remote socket created (fd=%d)\n", remote_fd);

    // Setup the destination address:
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(port);

    // Resolve the hostname to an IP:
    printf("[HTTPS] Resolving host: '%s'...\n", host);
    if (resolve_host(host, &dest) < 0) {
        printf("[HTTPS] ERROR: Failed to resolve host '%s'\n", host);
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTPS] Host resolved successfully\n");

    // Connect to the remote server:
    printf("[HTTPS] Connecting to remote %s:%d...\n", host, port);
    if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("[HTTPS] ERROR: tunnel connect failed");
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTPS] Connected to remote %s:%d\n", host, port);

    // Tell the client the tunnel is ready:
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

    // Blindly forward bytes in both directions:
    fd_set fds;
    char tunnel_buf[BUFFER_SIZE];
    int bytes;
    int total_client_to_remote = 0;
    int total_remote_to_client = 0;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(remote_fd, &fds);

        // Wait for data on either socket
        int max_fd = remote_fd > client_fd ? remote_fd : client_fd;
        printf("[HTTPS] select() waiting for data...\n");
        int sel = select(max_fd + 1, &fds, NULL, NULL, NULL);
        if (sel < 0) {
            perror("[HTTPS] ERROR: select failed");
            break;
        }
        printf("[HTTPS] select() returned %d\n", sel);

        // Client → Remote
        if (FD_ISSET(client_fd, &fds)) {
            bytes = recv(client_fd, tunnel_buf, sizeof(tunnel_buf), 0);
            if (bytes <= 0) {
                printf("[HTTPS] Client disconnected (recv returned %d)\n", bytes);
                break;
            }
            total_client_to_remote += bytes;
            printf("[HTTPS] Client → Remote: %d bytes (total: %d)\n", bytes, total_client_to_remote);
            int s = send(remote_fd, tunnel_buf, bytes, 0);
            if (s < 0) {
                perror("[HTTPS] ERROR: send to remote failed");
                break;
            }
        }

        // Remote → Client
        if (FD_ISSET(remote_fd, &fds)) {
            bytes = recv(remote_fd, tunnel_buf, sizeof(tunnel_buf), 0);
            if (bytes <= 0) {
                printf("[HTTPS] Remote disconnected (recv returned %d)\n", bytes);
                break;
            }
            total_remote_to_client += bytes;
            printf("[HTTPS] Remote → Client: %d bytes (total: %d)\n", bytes, total_remote_to_client);
            int s = send(client_fd, tunnel_buf, bytes, 0);
            if (s < 0) {
                perror("[HTTPS] ERROR: send to client failed");
                break;
            }
        }
    }

    printf("[HTTPS] Tunnel closed for %s:%d | C→R: %d bytes | R→C: %d bytes\n",
           host, port, total_client_to_remote, total_remote_to_client);
    close(remote_fd);
    close(client_fd);
}

// Handles plain HTTP requests:
void handle_http(int client_fd, char *buffer, int bytes_received) {

    printf("[HTTP] Entered handle_http\n");

    // Parse the host and port from the HTTP request:
    char host[256];
    int port;
    parse_host(buffer, host, &port);
    printf("[HTTP] Parsed host: '%s', port: %d\n", host, port);

    if (strlen(host) == 0) {
        printf("[HTTP] ERROR: host is empty, cannot forward\n");
        close(client_fd);
        return;
    }

    // Create a fresh remote socket:
    printf("[HTTP] Creating remote socket...\n");
    int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        perror("[HTTP] ERROR: remote socket failed");
        close(client_fd);
        return;
    }
    printf("[HTTP] Remote socket created (fd=%d)\n", remote_fd);

    // Setup the destination address:
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(port);

    // Resolve the hostname to an IP:
    printf("[HTTP] Resolving host: '%s'...\n", host);
    if (resolve_host(host, &dest) < 0) {
        printf("[HTTP] ERROR: Failed to resolve host '%s'\n", host);
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTP] Host resolved successfully\n");

    // Connect to the remote server:
    printf("[HTTP] Connecting to remote %s:%d...\n", host, port);
    if (connect(remote_fd, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        perror("[HTTP] ERROR: connect failed");
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTP] Connected to remote %s:%d\n", host, port);

    // Forward the HTTP request to the remote server:
    printf("[HTTP] Forwarding %d bytes to remote...\n", bytes_received);
    if (send(remote_fd, buffer, bytes_received, 0) < 0) {
        perror("[HTTP] ERROR: send to remote failed");
        close(remote_fd);
        close(client_fd);
        return;
    }
    printf("[HTTP] Request forwarded successfully\n");

    // Receive the response and forward back to client:
    char response[BUFFER_SIZE];
    int response_bytes;
    int total = 0;
    printf("[HTTP] Waiting for response from remote...\n");
    while ((response_bytes = recv(remote_fd, response, sizeof(response), 0)) > 0) {
        total += response_bytes;
        printf("[HTTP] Received %d bytes from remote (total: %d), forwarding to client...\n",
               response_bytes, total);
        int s = send(client_fd, response, response_bytes, 0);
        if (s < 0) {
            perror("[HTTP] ERROR: send to client failed");
            break;
        }
    }
    printf("[HTTP] Done. Total response: %d bytes\n", total);

    close(remote_fd);
    close(client_fd);
    printf("[HTTP] Sockets closed\n");
}

int main() {
    int server_fd;
    struct sockaddr_in address;

    printf("[MAIN] Starting proxy...\n");

    // Create the "client <=> proxy" server socket:
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[MAIN] ERROR: socket failed");
        return 1;
    }
    printf("[MAIN] Server socket created (fd=%d)\n", server_fd);

    // Allow port reuse so we can restart the proxy quickly
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    printf("[MAIN] SO_REUSEADDR set\n");

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("[MAIN] ERROR: bind failed");
        return 1;
    }
    printf("[MAIN] Bound to port %d\n", PORT);

    // Listen for connections:
    if (listen(server_fd, 10) < 0) {
        perror("[MAIN] ERROR: listen failed");
        return 1;
    }

    printf("[MAIN] Proxy listening on port %d...\n", PORT);

    while (1) {
        printf("[MAIN] Waiting for connection...\n");

        // Accept incoming client connections:
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("[MAIN] ERROR: accept failed");
            continue;
        }
        printf("[MAIN] Accepted connection (client_fd=%d)\n", client_fd);

        // Fork a child to handle this connection:
        pid_t pid = fork();
        if (pid < 0) {
            perror("[MAIN] ERROR: fork failed");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // Child process handles this connection:
            printf("[CHILD pid=%d] Handling new connection\n", getpid());
            close(server_fd);

            // Read the HTTP request from the client:
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));
            printf("[CHILD pid=%d] Waiting to recv from client...\n", getpid());
            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                printf("[CHILD pid=%d] ERROR: recv returned %d, closing\n", getpid(), bytes_received);
                close(client_fd);
                exit(0);
            }
            printf("[CHILD pid=%d] Received %d bytes from client\n", getpid(), bytes_received);
            printf("[CHILD pid=%d] Request:\n%s\n", getpid(), buffer);

            // Detect if it's HTTPS (CONNECT) or plain HTTP:
            if (strncmp(buffer, "CONNECT", 7) == 0) {
                printf("[CHILD pid=%d] Detected HTTPS CONNECT request\n", getpid());
                handle_https_tunnel(client_fd, buffer);
            } else {
                printf("[CHILD pid=%d] Detected plain HTTP request\n", getpid());
                handle_http(client_fd, buffer, bytes_received);
            }

            printf("[CHILD pid=%d] Done, exiting\n", getpid());
            exit(0);
        }

        // Parent closes its copy and loops back to accept:
        printf("[MAIN] Forked child pid=%d, closing parent's client_fd\n", pid);
        close(client_fd);

        // Reap any finished child processes to avoid zombies:
        waitpid(-1, NULL, WNOHANG);
    }

    close(server_fd);
    return 0;
}