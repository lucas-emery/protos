#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "requestParser.h"
#include "message.h"
#include "lib.h"

#define PORT_NUMBER 8090
#define CLIENT_BLOCK 5

typedef enum method {CONNECT, GET, POST, HEAD} method_t;

typedef struct {
    char * hostname;
    method_t method;
} request_t;

void serveClient(void*);

int main(int argc, char const *argv[]) {

    int mSock, clientCount = 0;
    socklen_t clilen;
    pthread_t * clients = NULL;

    struct sockaddr_in *my_addr = malloc(sizeof(struct sockaddr_in));

    mSock = socket(AF_INET, SOCK_STREAM,0);
    bzero((char *) my_addr, sizeof(struct sockaddr_in));
    my_addr->sin_family = AF_INET;
    my_addr->sin_port = htons(PORT_NUMBER);
    my_addr->sin_addr.s_addr = INADDR_ANY;

    int enable = 1;
    if (setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        DieWithSystemMessage("setsockopt(SO_REUSEADDR) failed");

    if (bind(mSock, (struct sockaddr *)my_addr, sizeof(struct sockaddr_in)) < 0)
        DieWithUserMessage("ded","ERROR on binding");

    listen(mSock,5);


    while(1){
        if(clientCount % CLIENT_BLOCK == 0){
            clients = realloc(clients, (clientCount + CLIENT_BLOCK)*sizeof(pthread_t));
        }
        pthread_create(&clients[clientCount], NULL, serveClient, (void *) accept(mSock, malloc(sizeof(struct sockaddr_in)), &clilen));
        clientCount++;
    }

    close(mSock);

    return 0;
}
