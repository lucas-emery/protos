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

#define PORT_NUMBER 8085
#define CLIENT_BLOCK 5

typedef enum method {CONNECT, GET, POST, HEAD} method_t;

typedef struct {
    char * hostname;
    method_t method;
} request_t;

struct sockaddr_in* findIp(char* hostname);
void connectToOrigin(struct sockaddr_in* host, char* content, int length);
void serveClient(void*);

int main(int argc, char const *argv[]) {

    int mSock, clientCount = 0;
    socklen_t clilen;
    char * inBuffer = malloc(BUFF_SIZE);
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

    if (bind(mSock, my_addr, sizeof(struct sockaddr_in)) < 0)
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

struct sockaddr_in* findIp(char* hostname) {
    char ip[100];
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *host;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
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
    return host;
}

void connectToOrigin(struct sockaddr_in* host, char* content, int length) {
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (s < 0)
        DieWithSystemMessage("socket() failed");

    host->sin_port = htons(80);

    if(connect(s, (struct sockaddr*)host, sizeof(*host)) < 0)
        DieWithSystemMessage("connect() failed");

    size_t numBytes = send(s, content, length, 0);

    if (numBytes < 0)
        DieWithSystemMessage("send() failed");
    else if (numBytes != length)
        DieWithUserMessage("send()", "sent unexpected number of bytes");

    unsigned int totalBytesRcvd = 0; // Count of total bytes received
    char* buffer = malloc(BUFF_SIZE + 1);
    int i = 0;

    // while (totalBytesRcvd < length) {
        /* Receive up to the buffer size (minus 1 to leave space for
         a null terminator) bytes from the sender */
        numBytes = recv(s, buffer, BUFF_SIZE, 0);

        if (numBytes < 0)
          DieWithSystemMessage("recv() failed");
        else if (numBytes == 0)
          DieWithUserMessage("recv()", "connection closed prematurely");

        totalBytesRcvd += numBytes; // Keep tally of total bytes
        buffer[numBytes] = '\0';    // Terminate the string!
        printf("%s", buffer);
        i++;
        buffer = realloc(buffer, BUFF_SIZE * i);
    // }

    free(buffer);
}
