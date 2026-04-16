#ifndef BLOCKLIST_H
#define BLOCKLIST_H

#define MAX_SITES 100
#define MAX_SITE_LENGTH 256
#define MAX_DATE_LENGTH 32

typedef struct
{
    int id;
    char site[MAX_SITE_LENGTH];
    char date_added[MAX_DATE_LENGTH];
} SiteEntry;

typedef struct
{
    SiteEntry sites[MAX_SITES];
    int count;
    int next_id;
} Blocklist;

void init_blocklist(Blocklist *list);
int load_blocklist(Blocklist *list, const char *filename);
int save_blocklist(const Blocklist *list, const char *filename);
int add_site(Blocklist *list, const char *site, SiteEntry *created_site);
int remove_site(Blocklist *list, const char *site);
void clear_sites(Blocklist *list);
int site_exists(const Blocklist *list, const char *site);

#endif
