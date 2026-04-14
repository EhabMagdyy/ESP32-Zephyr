
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/net/socket.h>

uint8_t nslookup(const char * hostname, struct zsock_addrinfo **results);
void addrinfo_results(struct zsock_addrinfo **results, char* myIPv4);
void http_get(int sock, char * hostname, char * url);
int connect_socket(struct zsock_addrinfo **results, uint16_t port);