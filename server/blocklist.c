#include "blocklist.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

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

void init_blocklist(Blocklist *list)
{
    list->count = 0;
    list->next_id = 1;
}

int load_blocklist(Blocklist *list, const char *filename)
{
    FILE *file = fopen(filename, "r");
    char line[512];
    init_blocklist(list);

    if (file == NULL)
    {
        return 0;
    }

    while (list->count < MAX_SITES && fgets(line, sizeof(line), file) != NULL)
    {
        SiteEntry *entry = &list->sites[list->count];
        int parsed_id = 0;
        char parsed_date[MAX_DATE_LENGTH] = "";
        char parsed_site[MAX_SITE_LENGTH] = "";

        line[strcspn(line, "\r\n")] = '\0';

        if (sscanf(line, "%d\t%31[^\t]\t%255[^\n]", &parsed_id, parsed_date, parsed_site) == 3)
        {
            entry->id = parsed_id;
            strncpy(entry->date_added, parsed_date, MAX_DATE_LENGTH - 1);
            entry->date_added[MAX_DATE_LENGTH - 1] = '\0';
            strncpy(entry->site, parsed_site, MAX_SITE_LENGTH - 1);
            entry->site[MAX_SITE_LENGTH - 1] = '\0';
            list->count++;
            if (parsed_id >= list->next_id)
            {
                list->next_id = parsed_id + 1;
            }
            continue;
        }

        if (strlen(line) > 0)
        {
            entry->id = list->next_id++;
            entry->date_added[0] = '\0';
            strncpy(entry->site, line, MAX_SITE_LENGTH - 1);
            entry->site[MAX_SITE_LENGTH - 1] = '\0';
            list->count++;
        }
    }

    fclose(file);
    return 1;
}

int save_blocklist(const Blocklist *list, const char *filename)
{
    FILE *file = fopen(filename, "w");
    int i;

    if (file == NULL)
    {
        return 0;
    }

    for (i = 0; i < list->count; i++)
    {
        fprintf(file, "%d\t%s\t%s\n",
                list->sites[i].id,
                list->sites[i].date_added,
                list->sites[i].site);
    }

    fclose(file);
    return 1;
}

int site_exists(const Blocklist *list, const char *site)
{
    int i;

    for (i = 0; i < list->count; i++)
    {
        if (strcmp(list->sites[i].site, site) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int add_site(Blocklist *list, const char *site, SiteEntry *created_site)
{
    int i;

    if (list->count >= MAX_SITES || site_exists(list, site))
    {
        return 0;
    }

    for (i = list->count; i > 0; i--)
    {
        list->sites[i] = list->sites[i - 1];
    }

    list->sites[0].id = list->next_id++;
    strncpy(list->sites[0].site, site, MAX_SITE_LENGTH - 1);
    list->sites[0].site[MAX_SITE_LENGTH - 1] = '\0';
    set_timestamp(list->sites[0].date_added, sizeof(list->sites[0].date_added));
    list->count++;

    if (created_site != NULL)
    {
        *created_site = list->sites[0];
    }

    return 1;
}

int remove_site(Blocklist *list, const char *site)
{
    int i;
    int j;

    for (i = 0; i < list->count; i++)
    {
        if (strcmp(list->sites[i].site, site) == 0)
        {
            for (j = i; j < list->count - 1; j++)
            {
                list->sites[j] = list->sites[j + 1];
            }

            list->count--;
            return 1;
        }
    }

    return 0;
}

void clear_sites(Blocklist *list)
{
    list->count = 0;
    list->next_id = 1;
}
