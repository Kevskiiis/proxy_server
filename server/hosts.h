#ifndef HOSTS_H
#define HOSTS_H

#include "blocklist.h"

int sync_hosts_file(const Blocklist *list, const char *hosts_filename);

#endif