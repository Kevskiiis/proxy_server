#ifndef REQ_HANDLER
#define REQ_HANDLER

#include <stdio.h>
#include <netdb.h> 
#include <string.h>
#include <stdlib.h>


void parse_host(const char *request, char *host_out, int *port_out) {
    *port_out = 80; // Default to HTTP port

    // Look for the "Host: " header in the request:
    const char *host_start = strstr(request, "Host: ");
    if (host_start == NULL) return;

    host_start += 6; // Skip past "Host: "

    // Copy the host into host_out until we hit \r or \n or :
    int i = 0;
    while (host_start[i] != '\r' && host_start[i] != '\n' && host_start[i] != ':' && host_start[i] != '\0') {
        host_out[i] = host_start[i];
        i++;
    }
    host_out[i] = '\0';

    // Check if a port was specified "Host: example.com:8080"
    if (host_start[i] == ':') {
        *port_out = atoi(&host_start[i + 1]);
    }
}

int resolve_host(const char *hostname, struct sockaddr_in *dest) {
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) {
        fprintf(stderr, "Could not resolve host: %s\n", hostname);
        return -1;
    }

    // Copy the resolved IP address into dest:
    memcpy(&dest->sin_addr, host->h_addr_list[0], host->h_length);
    return 0;
}

#endif