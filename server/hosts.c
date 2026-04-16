#include "hosts.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define HOSTS_BLOCK_START "# CS_GUARDIAN_START"
#define HOSTS_BLOCK_END   "# CS_GUARDIAN_END"
#define MAX_CONTENT_SIZE 65536

static EnforcementStatus last_status = {
    1,
    0,
    "Hosts file has not been synced yet.",
    ""
};

static void set_timestamp(char *buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm time_info;

#ifdef _WIN32
    gmtime_s(&time_info, &now);
#else
    gmtime_r(&now, &time_info);
#endif

    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &time_info);
}

static void append_managed_block(char *buffer, const Blocklist *list)
{
    int i;

    strcat(buffer, HOSTS_BLOCK_START);
    strcat(buffer, "\n");
    strcat(buffer, "# Managed by C version\n");

    for (i = 0; i < list->count; i++)
    {
        strcat(buffer, "127.0.0.1 ");
        strcat(buffer, list->sites[i].site);
        strcat(buffer, "\n");
        strcat(buffer, "::1 ");
        strcat(buffer, list->sites[i].site);
        strcat(buffer, "\n");

        if (strncmp(list->sites[i].site, "www.", 4) != 0)
        {
            strcat(buffer, "127.0.0.1 www.");
            strcat(buffer, list->sites[i].site);
            strcat(buffer, "\n");
            strcat(buffer, "::1 www.");
            strcat(buffer, list->sites[i].site);
            strcat(buffer, "\n");
        }
    }

    strcat(buffer, HOSTS_BLOCK_END);
    strcat(buffer, "\n");
}

int sync_hosts_file(const Blocklist *list, const char *hosts_filename)
{
    FILE *file = fopen(hosts_filename, "r");
    char content[MAX_CONTENT_SIZE] = "";
    char line[512];
    int inside_managed_block = 0;

    if (file != NULL)
    {
        while (fgets(line, sizeof(line), file) != NULL)
        {
            line[strcspn(line, "\r\n")] = '\0';

            if (strcmp(line, HOSTS_BLOCK_START) == 0)
            {
                inside_managed_block = 1;
                continue;
            }

            if (strcmp(line, HOSTS_BLOCK_END) == 0)
            {
                inside_managed_block = 0;
                continue;
            }

            if (!inside_managed_block)
            {
                strcat(content, line);
                strcat(content, "\n");
            }
        }

        fclose(file);
    }

    append_managed_block(content, list);

    file = fopen(hosts_filename, "w");
    if (file == NULL)
    {
        last_status.ok = 0;
        last_status.dns_flushed = 0;
        snprintf(last_status.message, sizeof(last_status.message),
                 "Failed to open hosts file for writing.");
        set_timestamp(last_status.synced_at, sizeof(last_status.synced_at));
        return 0;
    }

    fputs(content, file);
    fclose(file);

    last_status.ok = 1;
    last_status.dns_flushed = 0;
    snprintf(last_status.message, sizeof(last_status.message),
             "Synced %d blocked site(s) to hosts file.", list->count);
    set_timestamp(last_status.synced_at, sizeof(last_status.synced_at));

    return 1;
}

EnforcementStatus get_enforcement_status(void)
{
    return last_status;
}
