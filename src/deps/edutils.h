#ifndef EDUTILS_H_INCLUDED
#define EDUTILS_H_INCLUDED

#include <errno.h>

#define CHECK(X) ({int __val=(X); (__val ==-1 ? \
({fprintf(stderr,"ERROR (" __FILE__":%d) -- %s\n",__LINE__,strerror(errno));\
exit(-1);-1;}) : __val); })

#define CHECK_RETURN(X) ({int __val=(X); (__val <= 0 ? \
({if(__val == -1) fprintf(stderr,"ERROR (" __FILE__":%d) -- %s\n",__LINE__,strerror(errno));\
return __val;}) : __val); })

#define CHECK_PRINT(X) ({int __val=(X); (__val ==-1 ? \
({fprintf(stderr,"ERROR (" __FILE__":%d) -- %s\n",__LINE__,strerror(errno));\
-1;}) : __val); })

#define DEBUG_PRINT(X)    \
        if (DEBUG == 1) { \
                printf X; \
        };

char *clean_token(char *s, const int slen, char (*predicate)(const char));
char check_connection_status(int socket_fd);
struct sockaddr_in get_address(char *ip_address, int port);
void error();
int print_client_address(struct sockaddr_in client_address);

#endif