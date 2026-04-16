#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>

#include "blocklist.h"
#include "hosts.h"

#define BACKLOG 10
#define BUFFER_SIZE 8192

static int get_port(void)
{
    const char *port_value = getenv("CS_GUARDIAN_C_PORT");

    if (port_value == NULL || strlen(port_value) == 0)
    {
        port_value = getenv("PORT");
    }

    if (port_value == NULL || strlen(port_value) == 0)
    {
        return 8080;
    }

    return atoi(port_value);
}

static const char *get_data_file(void)
{
    const char *value = getenv("CS_GUARDIAN_C_DATA_FILE");

    if (value == NULL || strlen(value) == 0)
    {
        return "data/blocklist.txt";
    }

    return value;
}

static const char *get_hosts_file(void)
{
    const char *value = getenv("HOSTS_FILE_PATH");

    if (value == NULL || strlen(value) == 0)
    {
        value = getenv("CS_GUARDIAN_C_HOSTS_FILE");
    }

    if (value == NULL || strlen(value) == 0)
    {
#ifdef _WIN32
        return "C:\\Windows\\System32\\drivers\\etc\\hosts";
#else
        return "/etc/hosts";
#endif
    }

    return value;
}

static void append_json(char *buffer, size_t buffer_size, size_t *offset, const char *format, ...)
{
    va_list args;
    int written;

    if (*offset >= buffer_size)
    {
        return;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, buffer_size - *offset, format, args);
    va_end(args);

    if (written < 0)
    {
        return;
    }

    *offset += (size_t)written;
    if (*offset >= buffer_size)
    {
        *offset = buffer_size - 1;
    }
}

static void escape_json_string(const char *source, char *target, size_t target_size)
{
    size_t i = 0;
    size_t j = 0;

    while (source[i] != '\0' && j + 2 < target_size)
    {
        if (source[i] == '\\' || source[i] == '"')
        {
            target[j++] = '\\';
        }

        target[j++] = source[i++];
    }

    target[j] = '\0';
}

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

static int parse_site_from_body(const char *body, char *site, size_t site_size)
{
    const char *key = strstr(body, "\"site\"");
    const char *start;
    size_t index = 0;

    if (key == NULL)
    {
        site[0] = '\0';
        return 0;
    }

    start = strchr(key, ':');
    if (start == NULL)
    {
        site[0] = '\0';
        return 0;
    }

    start++;
    while (*start != '\0' && isspace((unsigned char)*start))
    {
        start++;
    }

    if (*start != '"')
    {
        site[0] = '\0';
        return 0;
    }

    start++;

    while (*start != '\0' && *start != '"' && index + 1 < site_size)
    {
        if (*start == '\\' && *(start + 1) != '\0')
        {
            start++;
        }

        site[index++] = *start++;
    }

    site[index] = '\0';
    return index > 0;
}

static void url_decode(const char *source, char *target, size_t target_size)
{
    size_t index = 0;

    while (*source != '\0' && index + 1 < target_size)
    {
        if (*source == '%' &&
            isxdigit((unsigned char)*(source + 1)) &&
            isxdigit((unsigned char)*(source + 2)))
        {
            char hex[3];
            hex[0] = *(source + 1);
            hex[1] = *(source + 2);
            hex[2] = '\0';
            target[index++] = (char)strtol(hex, NULL, 16);
            source += 3;
            continue;
        }

        if (*source == '+')
        {
            target[index++] = ' ';
            source++;
            continue;
        }

        target[index++] = *source++;
    }

    target[index] = '\0';
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
    url_decode(start, site, site_size);
}

static int normalize_site_input(const char *value, char *normalized, size_t normalized_size)
{
    char candidate[MAX_SITE_LENGTH];
    const char *start = value;
    const char *end;
    size_t length;
    size_t i;

    while (*start != '\0' && isspace((unsigned char)*start))
    {
        start++;
    }

    if (*start == '\0')
    {
        return 0;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1)))
    {
        end--;
    }

    length = (size_t)(end - start);
    if (length == 0 || length >= sizeof(candidate))
    {
        return 0;
    }

    strncpy(candidate, start, length);
    candidate[length] = '\0';

    if (strncmp(candidate, "http://", 7) == 0)
    {
        memmove(candidate, candidate + 7, strlen(candidate + 7) + 1);
    }
    else if (strncmp(candidate, "https://", 8) == 0)
    {
        memmove(candidate, candidate + 8, strlen(candidate + 8) + 1);
    }

    for (i = 0; candidate[i] != '\0'; i++)
    {
        if (candidate[i] == '/' || candidate[i] == '?' || candidate[i] == '#')
        {
            candidate[i] = '\0';
            break;
        }
    }

    if (strncmp(candidate, "www.", 4) == 0)
    {
        memmove(candidate, candidate + 4, strlen(candidate + 4) + 1);
    }

    length = strlen(candidate);
    if (length == 0 || length >= normalized_size)
    {
        return 0;
    }

    for (i = 0; i < length; i++)
    {
        char current = (char)tolower((unsigned char)candidate[i]);

        if (!(isalnum((unsigned char)current) || current == '.' || current == '-'))
        {
            return 0;
        }

        normalized[i] = current;
    }

    normalized[length] = '\0';
    return strchr(normalized, '.') != NULL;
}

static void build_sites_json(const Blocklist *list, char *body, size_t body_size)
{
    int i;
    size_t offset = 0;
    append_json(body, body_size, &offset, "{\"sites\":[");

    for (i = 0; i < list->count; i++)
    {
        char site[MAX_SITE_LENGTH * 2];
        char date_added[MAX_DATE_LENGTH * 2];

        escape_json_string(list->sites[i].site, site, sizeof(site));
        escape_json_string(list->sites[i].date_added, date_added, sizeof(date_added));

        append_json(body, body_size, &offset,
                    "{\"id\":%d,\"site\":\"%s\",\"dateAdded\":\"%s\"}",
                    list->sites[i].id,
                    site,
                    date_added);

        if (i < list->count - 1)
        {
            append_json(body, body_size, &offset, ",");
        }
    }

    append_json(body, body_size, &offset, "]}");
}

static void build_health_json(const Blocklist *list, const char *data_file, const char *hosts_file,
                              char *body, size_t body_size)
{
    char escaped_data_file[512];
    char escaped_hosts_file[512];
    char escaped_message[512];
    char escaped_synced_at[128];
    size_t offset = 0;
    EnforcementStatus enforcement = get_enforcement_status();

    escape_json_string(data_file, escaped_data_file, sizeof(escaped_data_file));
    escape_json_string(hosts_file, escaped_hosts_file, sizeof(escaped_hosts_file));
    escape_json_string(enforcement.message, escaped_message, sizeof(escaped_message));
    escape_json_string(enforcement.synced_at, escaped_synced_at, sizeof(escaped_synced_at));

    append_json(body, body_size, &offset,
                "{\"status\":\"ok\",\"service\":\"cs-guardian-c-api\","
                "\"enforcementMode\":\"hosts-file-c\",\"dataFile\":\"%s\","
                "\"hostsFile\":\"%s\",\"siteCount\":%d,"
                "\"enforcement\":{\"ok\":%s,\"message\":\"%s\","
                "\"dnsFlushed\":%s,\"syncedAt\":\"%s\"}}",
                escaped_data_file,
                escaped_hosts_file,
                list->count,
                enforcement.ok ? "true" : "false",
                escaped_message,
                enforcement.dns_flushed ? "true" : "false",
                escaped_synced_at);
}

static void handle_request(int client_fd, const char *request)
{
    char method[16];
    char path[256];
    char body[4096];
    char response_body[4096];
    char site[MAX_SITE_LENGTH];
    char normalized_site[MAX_SITE_LENGTH];
    const char *body_start;
    Blocklist list;
    const char *data_file = get_data_file();
    const char *hosts_file = get_hosts_file();

    load_blocklist(&list, data_file);

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
        build_health_json(&list, data_file, hosts_file, response_body, sizeof(response_body));
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
        if (!parse_site_from_body(body, site, sizeof(site)) ||
            !normalize_site_input(site, normalized_site, sizeof(normalized_site)))
        {
            send_response(client_fd, "400 Bad Request",
                          "{\"error\":\"Enter a valid hostname or URL.\"}");
            return;
        }

        if (!add_site(&list, normalized_site, NULL))
        {
            send_response(client_fd, "409 Conflict",
                          "{\"error\":\"That site is already blocked or list is full.\"}");
            return;
        }

        save_blocklist(&list, data_file);
        sync_hosts_file(&list, hosts_file);
        build_sites_json(&list, response_body, sizeof(response_body));
        send_response(client_fd, "201 Created", response_body);
        return;
    }

    if (strcmp(method, "DELETE") == 0 && strcmp(path, "/api/sites") == 0)
    {
        clear_sites(&list);
        save_blocklist(&list, data_file);
        sync_hosts_file(&list, hosts_file);
        send_response(client_fd, "200 OK", "{\"sites\":[]}");
        return;
    }

    if (strcmp(method, "DELETE") == 0 && strncmp(path, "/api/sites/", 11) == 0)
    {
        extract_site_from_path(path, site, sizeof(site));

        if (!normalize_site_input(site, normalized_site, sizeof(normalized_site)) ||
            !remove_site(&list, normalized_site))
        {
            send_response(client_fd, "404 Not Found",
                          "{\"error\":\"That site is not in the block list.\"}");
            return;
        }

        save_blocklist(&list, data_file);
        sync_hosts_file(&list, hosts_file);
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
    int port = get_port();
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int bytes_received;
    Blocklist startup_list;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return 1;
    }

    {
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

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

    load_blocklist(&startup_list, get_data_file());
    sync_hosts_file(&startup_list, get_hosts_file());

    printf("C blocklist API listening on port %d...\n", port);

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
