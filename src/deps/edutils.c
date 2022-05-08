#include "edutils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

char *clean_token(char *s, const int slen, char (*predicate)(const char)) {
        char *accumulator;
        int j;

        accumulator = (char *)calloc(slen, sizeof(char));

        for (int i = j = 0; i < slen; i++)
                if ((*predicate)(s[i]))
                        accumulator[j++] = s[i];

        accumulator[j] = '\0';

        return accumulator;
}

struct sockaddr_in get_address(char *ip_address, int port) {
        struct sockaddr_in server_address;

        memset((char *)&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);

        CHECK(inet_aton(ip_address, &server_address.sin_addr));
        server_address.sin_port = htons((unsigned short)port);
        return server_address;
}
