#include "hosts.h"
#include <stdio.h>
#include <string.h>

#define HOSTS_BLOCK_START "# CS_GUARDIAN_START"
#define HOSTS_BLOCK_END   "# CS_GUARDIAN_END"
#define MAX_CONTENT_SIZE 65536

static void append_managed_block(char *buffer, const Blocklist *list)
{
    int i;

    strcat(buffer, HOSTS_BLOCK_START);
    strcat(buffer, "\n");
    strcat(buffer, "# Managed by C version\n");

    for (i = 0; i < list->count; i++)
    {
        strcat(buffer, "127.0.0.1 ");
        strcat(buffer, list->sites[i]);
        strcat(buffer, "\n");

        if (strncmp(list->sites[i], "www.", 4) != 0)
        {
            strcat(buffer, "127.0.0.1 www.");
            strcat(buffer, list->sites[i]);
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
        return 0;
    }

    fputs(content, file);
    fclose(file);

    return 1;
}