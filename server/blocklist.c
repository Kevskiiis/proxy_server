#include "blocklist.h"
#include <stdio.h>
#include <string.h>

// Count how many sites are blocked: 
void init_blocklist(Blocklist *list)
{
    list->count = 0;
}

// Load the entire blocklist: 
int load_blocklist(Blocklist *list, const char *filename)
{
    FILE *file = fopen(filename, "r");
    init_blocklist(list);

    if (file == NULL)
    {
        return 0;
    }

    while (list->count < MAX_SITES &&
           fgets(list->sites[list->count], MAX_SITE_LENGTH, file) != NULL)
    {
        list->sites[list->count][strcspn(list->sites[list->count], "\r\n")] = '\0';

        if (strlen(list->sites[list->count]) > 0)
        {
            list->count++;
        }
    }

    fclose(file);
    return 1;
}

// Save the blocklist:
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
        fprintf(file, "%s\n", list->sites[i]);
    }

    fclose(file);
    return 1;
}

// Check if site exists: 
int site_exists(const Blocklist *list, const char *site)
{
    int i;

    for (i = 0; i < list->count; i++)
    {
        if (strcmp(list->sites[i], site) == 0)
        {
            return 1;
        }
    }

    return 0;
}

// Add a site: 
int add_site(Blocklist *list, const char *site)
{
    if (list->count >= MAX_SITES || site_exists(list, site))
    {
        return 0;
    }

    strncpy(list->sites[list->count], site, MAX_SITE_LENGTH - 1);
    list->sites[list->count][MAX_SITE_LENGTH - 1] = '\0';
    list->count++;

    return 1;
}

// Remove site: 
int remove_site(Blocklist *list, const char *site)
{
    int i;
    int j;

    for (i = 0; i < list->count; i++)
    {
        if (strcmp(list->sites[i], site) == 0)
        {
            for (j = i; j < list->count - 1; j++)
            {
                strcpy(list->sites[j], list->sites[j + 1]);
            }

            list->count--;
            return 1;
        }
    }

    return 0;
}

// Clear site: 
void clear_sites(Blocklist *list)
{
    list->count = 0;
}