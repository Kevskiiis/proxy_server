#ifndef DATABASE_H
#define DATABASE_H

#define DB_PATH "database/managed_sites.db"
#define MAX_SITES 256
#define MAX_SITE_LENGTH 256

typedef struct {
    int  id;
    char site[MAX_SITE_LENGTH];
    char date_added[64];
} SiteRecord;

typedef struct {
    SiteRecord records[MAX_SITES];
    int count;
} SiteList;

int  db_init();
int  db_get_all_sites(SiteList *list);
int  db_add_site(const char *site);
int  db_remove_site(const char *site);
int  db_clear_sites();
int  db_site_exists(const char *site);

#endif