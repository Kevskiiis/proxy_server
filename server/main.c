#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "blocklist.h"
#include "hosts.h"

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 8192
#define DATA_FILE "data/blocklist.txt"
#define HOSTS_FILE "data/hosts_test.txt"

static void send_response(int client_fd, const char *status, const char *body)
{
    char response[BUFFER_SIZE];
    int body_length = (int)strlen(body);

    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, body_length, body);

    send(client_fd, response, strlen(response), 0);
}

static void parse_site_from_body(const char *body, char *site, size_t site_size)
{
    const char *prefix = "site=";
    const char *start = strstr(body, prefix);

    if (start == NULL)
    {
        site[0] = '\0';
        return;
    }

    start += strlen(prefix);
    strncpy(site, start, site_size - 1);
    site[site_size - 1] = '\0';

    site[strcspn(site, "&\r\n")] = '\0';
}

static void extract_site_from_path(const char *path, char *site, size_t site_size)
{
    const char *prefix = "/api/sites/";
    const char *start = strstr(path, prefix);

    if (start == NULL)
    {
        site[0] = '\0';
        return;
    }

    start += strlen(prefix);
    strncpy(site, start, site_size - 1);
    site[site_size - 1] = '\0';
}

static void build_sites_json(const Blocklist *list, char *body, size_t body_size)
{
    int i;
    snprintf(body, body_size, "{\"sites\":[");

    for (i = 0; i < list->count; i++)
    {
        strcat(body, "\"");
        strcat(body, list->sites[i]);
        strcat(body, "\"");

        if (i < list->count - 1)
        {
            strcat(body, ",");
        }
    }

    strcat(body, "]}");
}

static void handle_request(int client_fd, const char *request)
{
    char method[16];
    char path[256];
    char body[4096];
    char response_body[4096];
    char site[MAX_SITE_LENGTH];
    const char *body_start;
    Blocklist list;

    load_blocklist(&list, DATA_FILE);

    sscanf(request, "%15s %255s", method, path);

    body_start = strstr(request, "\r\n\r\n");
    if (body_start != NULL)
    {
        body_start += 4;
        strncpy(body, body_start, sizeof(body) - 1);
        body[sizeof(body) - 1] = '\0';
    }
    else
    {
        body[0] = '\0';
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/health") == 0)
    {
        snprintf(response_body, sizeof(response_body),
                 "{\"status\":\"ok\",\"service\":\"c-blocklist-api\",\"siteCount\":%d}",
                 list.count);
        send_response(client_fd, "200 OK", response_body);
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/sites") == 0)
    {
        build_sites_json(&list, response_body, sizeof(response_body));
        send_response(client_fd, "200 OK", response_body);
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/sites") == 0)
    {
        parse_site_from_body(body, site, sizeof(site));

        if (strlen(site) == 0)
        {
            send_response(client_fd, "400 Bad Request",
                          "{\"error\":\"Missing site field.\"}");
            return;
        }

        if (!add_site(&list, site))
        {
            send_response(client_fd, "409 Conflict",
                          "{\"error\":\"That site is already blocked or list is full.\"}");
            return;
        }

        save_blocklist(&list, DATA_FILE);
        sync_hosts_file(&list, HOSTS_FILE);
        build_sites_json(&list, response_body, sizeof(response_body));
        send_response(client_fd, "201 Created", response_body);
        return;
    }

    if (strcmp(method, "DELETE") == 0 && strcmp(path, "/api/sites") == 0)
    {
        clear_sites(&list);
        save_blocklist(&list, DATA_FILE);
        sync_hosts_file(&list, HOSTS_FILE);
        send_response(client_fd, "200 OK", "{\"sites\":[]}");
        return;
    }

    if (strcmp(method, "DELETE") == 0 && strncmp(path, "/api/sites/", 11) == 0)
    {
        extract_site_from_path(path, site, sizeof(site));

        if (!remove_site(&list, site))
        {
            send_response(client_fd, "404 Not Found",
                          "{\"error\":\"That site is not in the block list.\"}");
            return;
        }

        save_blocklist(&list, DATA_FILE);
        sync_hosts_file(&list, HOSTS_FILE);
        build_sites_json(&list, response_body, sizeof(response_body));
        send_response(client_fd, "200 OK", response_body);
        return;
    }

    send_response(client_fd, "404 Not Found", "{\"error\":\"Route not found.\"}");
}

int main(void)
{
    int server_fd;
    int client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen failed");
        close(server_fd);
        return 1;
    }

    printf("C blocklist API listening on port %d...\n", PORT);

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept failed");
            continue;
        }

        bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';
            handle_request(client_fd, buffer);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}