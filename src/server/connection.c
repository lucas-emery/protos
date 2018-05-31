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
#include <pthread.h>
#include "requestParser.h"
#include "message.h"

#define SERV_BLOCK 5

typedef struct {
    int servCount;
    int *servSocks;
    char **servIps;
} connectionCDT;

typedef connectionCDT* connectionADT;

struct sockaddr_in* findIp(char* hostname);
int getservSocks(connectionADT connection, char * buffer, int length);
int connectToServer(struct sockaddr_in* host);
void serveClient(void * arg);
void * transform(void*);

void serveClient(void * arg){
    int cliSock = (int) arg;
    // printf("I am a thread and this is my cliSock: %d\n", cliSock);

    char * inBuffer = malloc(BUFF_SIZE);
    int nfds = cliSock;

    connectionADT connection = malloc(sizeof(connectionCDT));
    connection->servCount = 0;
    connection->servIps = malloc(sizeof(char*));

    if (cliSock < 0)
      DieWithSystemMessage("ERROR on accept");

    fd_set fdset;

    while(1){

        FD_ZERO( &fdset );
	    FD_SET( cliSock, &fdset );
        nfds = cliSock;
        for(int i = 0; i < connection->servCount ; i++){
            if(connection->servSocks[i] > nfds)
                nfds = connection->servSocks[i];
	        FD_SET( connection->servSocks[i], &fdset );
        }
        nfds += 1;

        int aux = 0;

        int r = select( nfds, &fdset, (fd_set*) 0, (fd_set*) 0, NULL);
        if( r == 0  )
            DieWithSystemMessage("TIMEOUT");

        bzero(inBuffer,BUFF_SIZE);
        if(FD_ISSET(cliSock, &fdset)){
            int destSock;
            aux = recv(cliSock, inBuffer, BUFF_SIZE,0);
            if(aux > 0){
                destSock = getservSocks(connection, inBuffer, aux);
                if(destSock >= 0){
                    send(destSock, inBuffer, aux, 0);
                } else {
                    send(cliSock, "HTTP/1.1 405 Method Not Allowed\r\n\r\n", strlen("HTTP/1.1 405 Method Not Allowed\r\n\r\n"),0);
                }
            }
        } else {
            int srcSock = -1;
            for(int i = 0; i < connection->servCount ; i++){
                if(FD_ISSET(connection->servSocks[i], &fdset)){
                    srcSock = connection->servSocks[i];
                }
            }
            if(srcSock < 0)
                break;

            pthread_t t;
            char* bin = "./echo";
            int* in = malloc(sizeof(int));
            int* out = malloc(sizeof(int));
            *in = srcSock;
            *out = cliSock;
            void * arg[3];
            arg[0] = (void*) bin;
            arg[1] = (void*) in;
            arg[2] = (void*) out;
            pthread_create(&t, 0, transform, arg);
            pthread_join(t,NULL);
            /*
            aux = recv(srcSock, inBuffer, BUFF_SIZE,0);
            if(aux > 0){
                send(cliSock, inBuffer, aux, 0);
            }
            */
        }
    }
}

int getservSocks(connectionADT connection, char * buffer, int length){
    char *hostName = parseRequest(buffer, length);
    if(hostName == NULL)
        return -1;
    struct sockaddr_in *serv_addr = findIp(hostName);

    if(serv_addr == NULL)
        return -1;

    for(int i = 0; i < connection->servCount; i++){
        if(strcmp(connection->servIps[i],hostName)==0){
            //free(hostName);
            //free(serv_addr);
            return connection->servSocks[i];
        }
    }
    if(connection->servCount % SERV_BLOCK == 0){
        connection->servSocks = realloc(connection->servSocks, (connection->servCount + SERV_BLOCK)*sizeof(int));
        connection->servIps = realloc(connection->servIps, (connection->servCount + SERV_BLOCK)*sizeof(char*));
    }

    connection->servSocks[connection->servCount] = connectToServer(serv_addr);
    connection->servIps[connection->servCount] = hostName;

    return connection->servSocks[connection->servCount++];
}

int connectToServer(struct sockaddr_in* host){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0){
        DieWithSystemMessage("socket() failed");
    }

    host->sin_port = htons(80);

    if(connect(sockfd, (struct sockaddr*)host, sizeof(*host)) < 0){
        DieWithSystemMessage("connect() failed");
    }

    return sockfd;
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
