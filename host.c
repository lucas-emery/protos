#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<errno.h>
#include<netdb.h>
#include<arpa/inet.h>

#define BUFF_SIZE 100

typedef enum request_state {NO_HOST, HOST, WITH_HOST, O, S, T, DOT} RequestState;

struct sockaddr_in* findByIp(char* hostname);

int main(int argc, char const *argv[]) {

    char c;
    RequestState state =  NO_HOST;
    char host[2048];
    char* buffer = malloc(0);
    int counterHost = 0;
    int counter = 0;
    int i;

    for(i = 0; (c = getchar()) != EOF; i++) {

        if(i % BUFF_SIZE == 0){
            counter++;
            buffer = realloc(buffer, BUFF_SIZE * counter);
        }

        *(buffer + i) = c;

        switch (state) {
            case NO_HOST:
                if(c == 'H')
                    state = O;
            break;

            case HOST:
                if(c == '\n')
                    state = WITH_HOST;
                else if (c != ' ')
                    host[counterHost++] = c;
            break;

            case O:
                if(c == 'o')
                    state = S;
                else
                    state = NO_HOST;
            break;

            case S:
                if(c == 's')
                    state = T;
                else
                    state = NO_HOST;
            break;

            case T:
                if(c == 't')
                    state = DOT;
                else
                    state = NO_HOST;
            break;

            case DOT:
                if(c == ':')
                    state = HOST;
                else
                    state = NO_HOST;
            break;

            case WITH_HOST:
                break;
        }
    }

    host[counterHost] = 0;
    buffer[i] = 0;
    printf("%s\n", buffer);
    printf("%s\n", host);
    free(buffer);

    findByIp(host);
    return 0;
}

struct sockaddr_in* findByIp(char* hostname) {
    char ip[100];
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *host;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ( (rv = getaddrinfo( hostname , "http" , &hints , &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return NULL;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p -> ai_next) {
        host = (struct sockaddr_in *) p -> ai_addr;
        strcpy(ip , inet_ntoa( host -> sin_addr ) );
    }

    freeaddrinfo(servinfo); // all done with this structure
    printf("%s\n", ip);
    return host;
}
