#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "hosts.h"
#include "database/database.h"

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 8192
#define HOSTS_FILE "data/hosts_test.txt"

static void send_response(int client_fd, const char *status, const char *body)
{
    char response[BUFFER_SIZE];
    int body_length = (int)strlen(body);

    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, body_length, body);

    send(client_fd, response, strlen(response), 0);
}

// Parses "site":"example.com" from JSON body:
static void parse_site_from_body(const char *body, char *site, size_t site_size)
{
    const char *key = "\"site\"";
    const char *key_pos = strstr(body, key);

    if (key_pos == NULL)
    {
        site[0] = '\0';
        return;
    }

    // Move past "site" to the colon:
    const char *colon = strchr(key_pos + strlen(key), ':');
    if (colon == NULL)
    {
        site[0] = '\0';
        return;
    }

    // Move past the colon and any whitespace to the opening quote:
    const char *open_quote = strchr(colon + 1, '"');
    if (open_quote == NULL)
    {
        site[0] = '\0';
        return;
    }

    open_quote++; // Move past the opening quote

    // Copy until closing quote:
    const char *close_quote = strchr(open_quote, '"');
    if (close_quote == NULL)
    {
        site[0] = '\0';
        return;
    }

    size_t len = close_quote - open_quote;
    if (len >= site_size) len = site_size - 1;

    strncpy(site, open_quote, len);
    site[len] = '\0';
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

// Builds JSON array of full site objects:
// {"sites":[{"id":1,"site":"example.com","dateAdded":"2026-04-23"}]}
static void build_sites_json(const SiteList *list, char *body, size_t body_size)
{
    int i;
    char entry[512];

    snprintf(body, body_size, "{\"sites\":[");

    for (i = 0; i < list->count; i++)
    {
        snprintf(entry, sizeof(entry),
                 "{\"id\":%d,\"site\":\"%s\",\"dateAdded\":\"%s\"}",
                 list->records[i].id,
                 list->records[i].site,
                 list->records[i].date_added);

        strcat(body, entry);

        if (i < list->count - 1)
        {
            strcat(body, ",");
        }
    }

    strcat(body, "]}");
}

// Syncs the hosts file from the DB:
static void sync_hosts_from_db()
{
    SiteList list;
    Blocklist bl;
    int i;

    db_get_all_sites(&list);

    // Convert SiteList into Blocklist for sync_hosts_file:
    bl.count = list.count;
    for (i = 0; i < list.count; i++)
    {
        strncpy(bl.sites[i], list.records[i].site, MAX_SITE_LENGTH - 1);
        bl.sites[i][MAX_SITE_LENGTH - 1] = '\0';
    }

    sync_hosts_file(&bl, HOSTS_FILE);
}

static void handle_request(int client_fd, const char *request)
{
    char method[16];
    char path[256];
    char body[4096];
    char response_body[8192];
    char site[MAX_SITE_LENGTH];
    const char *body_start;
    SiteList list;

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

    // GET /api/health
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/health") == 0)
    {
        db_get_all_sites(&list);
        snprintf(response_body, sizeof(response_body),
                 "{\"status\":\"ok\",\"service\":\"c-blocklist-api\",\"siteCount\":%d}",
                 list.count);
        send_response(client_fd, "200 OK", response_body);
        return;
    }

    // GET /api/sites
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/sites") == 0)
    {
        db_get_all_sites(&list);
        build_sites_json(&list, response_body, sizeof(response_body));
        send_response(client_fd, "200 OK", response_body);
        return;
    }

    // POST /api/sites
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/sites") == 0)
    {
        parse_site_from_body(body, site, sizeof(site));

        if (strlen(site) == 0)
        {
            send_response(client_fd, "400 Bad Request",
                          "{\"error\":\"Missing site field.\"}");
            return;
        }

        if (!db_add_site(site))
        {
            send_response(client_fd, "409 Conflict",
                          "{\"error\":\"That site is already blocked.\"}");
            return;
        }

        sync_hosts_from_db();
        db_get_all_sites(&list);
        build_sites_json(&list, response_body, sizeof(response_body));
        send_response(client_fd, "201 Created", response_body);
        return;
    }

    // DELETE /api/sites  (clear all)
    if (strcmp(method, "DELETE") == 0 && strcmp(path, "/api/sites") == 0)
    {
        db_clear_sites();
        sync_hosts_from_db();
        send_response(client_fd, "200 OK", "{\"sites\":[]}");
        return;
    }

    // DELETE /api/sites/:site
    if (strcmp(method, "DELETE") == 0 && strncmp(path, "/api/sites/", 11) == 0)
    {
        extract_site_from_path(path, site, sizeof(site));

        if (!db_remove_site(site))
        {
            send_response(client_fd, "404 Not Found",
                          "{\"error\":\"That site is not in the block list.\"}");
            return;
        }

        sync_hosts_from_db();
        db_get_all_sites(&list);
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

    // Initialize the DB and table on startup:
    if (!db_init())
    {
        fprintf(stderr, "Failed to initialize database.\n");
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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