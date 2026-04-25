#ifndef BLOCKLIST_H
#define BLOCKLIST_H

#define MAX_SITES 256
#define MAX_SITE_LENGTH 256

typedef struct
{
    char sites[MAX_SITES][MAX_SITE_LENGTH];
    int count;
} Blocklist;

void init_blocklist(Blocklist *list);
int load_blocklist(Blocklist *list, const char *filename);
int save_blocklist(const Blocklist *list, const char *filename);
int add_site(Blocklist *list, const char *site);
int remove_site(Blocklist *list, const char *site);
void clear_sites(Blocklist *list);
int site_exists(const Blocklist *list, const char *site);

#endif