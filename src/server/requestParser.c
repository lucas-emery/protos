#include "requestParser.h"

#define HOST_SIZE 1024

typedef enum request_state {
    METHOD,
    NO_HOST,
    HOST,
    WITH_HOST,
    O,
    S,
    T,
    DOT
} request_state_t;

void parseRequest(const char* inBuffer, int n, request_t* request) {

    char c;
    request_state_t state =  METHOD;
    char* host = malloc(0);
    char method[50];
    int methodCounter = 0;
    int hostCounter = 0;
    int buffCounter = 0;

    for(int i = 0; i < n; i++) {
        c = *(inBuffer + i);

        switch (state) {

            case METHOD:
                if(c == ' '){
                    state = NO_HOST;
                    method[methodCounter] = 0;
                } else
                    method[methodCounter++] = c;
            break;

            case NO_HOST:
                if(c == 'H')
                    state = O;
            break;

            case HOST:
                if(c == '\r' || c == ':')
                    state = WITH_HOST;
                else if (c != ' ') {

                    if(hostCounter % HOST_SIZE == 0){
                        buffCounter++;
                        host = realloc(host, buffCounter * HOST_SIZE);
                    }

                    host[hostCounter++] = c;
                }

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

    host[hostCounter] = 0;

    if(strcmp(method, "CONNECT") == 0)
        request->method = CONNECT;
    else if(strcmp(method, "GET") == 0)
        request->method = GET;
    else if(strcmp(method, "POST") == 0)
        request->method = POST;
    else if(strcmp(method, "HEAD") == 0)
        request->method = HEAD;

    request->hostname = host;
}
