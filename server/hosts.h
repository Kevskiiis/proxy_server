#ifndef HOSTS_H
#define HOSTS_H

#include "blocklist.h"

typedef struct
{
    int ok;
    int dns_flushed;
    char message[256];
    char synced_at[MAX_DATE_LENGTH];
} EnforcementStatus;

int sync_hosts_file(const Blocklist *list, const char *hosts_filename);
EnforcementStatus get_enforcement_status(void);

#endif
