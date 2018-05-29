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

#define BUFF_SIZE 2048
#define SERV_BLOCK 5

typedef struct {
    int servCount;
    int *servSocks;
    char **servIps;
} connectionCDT;

typedef connectionCDT* connectionADT;

void serveClient(void * arg){
    int cliSock = (int) arg;
    printf("I am a thread and this is my cliSock: %d\n", cliSock);

    char * inBuffer = malloc(BUFF_SIZE);
    int read, nfds = cliSock;

    connectionADT connection = malloc(sizeof(connectionCDT));
    connection->servCount = 0;

    if (cliSock < 0)
      DieWithSystemMessage("ERROR on accept");

    struct timeval timeout;
    fd_set fdset;

    timeout.tv_sec = 30;
    timeout.tv_usec = 0;

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

        int r = select( nfds, &fdset, (fd_set*) 0, (fd_set*) 0, &timeout);
        if( r == 0  )
            DieWithSystemMessage("TIMEOUT");

        bzero(inBuffer,BUFF_SIZE);
        if(FD_ISSET(cliSock, &fdset)){
            int destSock;
            aux = recv(cliSock, inBuffer, BUFF_SIZE,0);
            printf("%s\n", inBuffer);
            if(aux > 0){
                destSock = getservSocks(connection, inBuffer, aux);
                printf("Sending to %d\n", destSock);
                send(destSock, inBuffer, aux, 0);
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
            aux = recv(srcSock, inBuffer, BUFF_SIZE,0);
            if(aux > 0){
                send(cliSock, inBuffer, aux, 0);
            }
        }
    }
}

int getservSocks(connectionADT connection, char * buffer, int length){
    char *hostName = parseRequest(buffer, length);
    struct sockaddr_in *serv_addr = findIp(hostName);
    for(int i = 0; i < connection->servCount; i++){
        if(strcmp(connection->servIps[i],hostName)==0){
            free(hostName);
            free(serv_addr);
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
    printf("didnt\n");
    host->sin_port = htons(80);
    printf("passed this]\n");

    if(connect(sockfd, (struct sockaddr*)host, sizeof(*host)) < 0){
        DieWithSystemMessage("connect() failed");
    }

    return sockfd;
}
