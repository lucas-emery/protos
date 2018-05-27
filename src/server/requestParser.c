#include "requestParser.h"

#define HOST_SIZE 1024

typedef enum request_state {NO_HOST,
    HOST,
    WITH_HOST,
    O,
    S,
    T,
    DOT
} RequestState;

char* parseRequest(const char* inBuffer, int n) {

    char c;
    RequestState state =  NO_HOST;
    char* host = malloc(HOST_SIZE);
    int counterHost = 0;
    int buffCounter = 1;

    for(int i = 0; i < n; i++) {
        c = *(inBuffer + i);

        switch (state) {
            case NO_HOST:
                if(c == 'H')
                    state = O;
            break;

            case HOST:
                if(c == '\r' || c == ':')
                    state = WITH_HOST;
                else if (c != ' ') {

                    if(counterHost % HOST_SIZE == 0){
                        host = realloc(host, buffCounter * HOST_SIZE);
                        buffCounter++;
                    }

                    host[counterHost++] = c;
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

    host[counterHost] = 0;

    return host;
}
