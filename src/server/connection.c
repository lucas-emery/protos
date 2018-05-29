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

#define SERV_BLOCK 5
#define SIZE 4096

typedef struct {
    int servCount;
    int *servSocks;
    char **servIps;
} connectionCDT;

typedef connectionCDT* connectionADT;

struct sockaddr_in* findIp(char* hostname);
char * readSocket(int sock, int * len);

void serveClient(void * arg){
    int cliSock = (int) arg;
    printf("I am a thread and this is my cliSock: %d\n", cliSock);
    int count = 0;

    connectionADT connection = malloc(sizeof(connectionCDT));
    connection->servCount = 0;

    if (cliSock < 0)
      DieWithSystemMessage("ERROR on accept");

    fd_set fdset;

    while(1){

        int nfds = prepareSelect(cliSock, connection, &fdset);

        int length = 0;

        char * buffer = NULL;

        int r = select( nfds, &fdset, (fd_set*) 0, (fd_set*) 0, NULL);
        if( r == 0  )
            DieWithSystemMessage("TIMEOUT");

        if(FD_ISSET(cliSock, &fdset)){
            int destSock;
            buffer = readSocket(cliSock, &length);
            if(length > 0){
                //printf("%s\n", buffer);
                destSock = getservSocks(connection, buffer, length);
                if(destSock >= 0){
                    writeSocket(destSock,buffer,length);
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
            buffer = readSocket(srcSock,&length);
            //printf("%s\n", buffer);
            if(length > 0){
                transform(buffer);
                printf("sending to client\n");
                writeSocket(cliSock,buffer,length);
            }
        }
        //free(buffer);
    }
}

void transform(char * body){

}

char * readSocket(int sock, int * len){
    int finished = 0, blockCount = 0, totalLength = 0, length = 0;
    char * inBuffer = malloc(BUFF_SIZE);
    char * buffer = NULL;

    //while(!finished){
        bzero(inBuffer,BUFF_SIZE);
        length = recv(sock, inBuffer, BUFF_SIZE,0);
        totalLength += length;

        if(totalLength >= blockCount*BUFF_SIZE){
            blockCount++;
            buffer = realloc(buffer, BUFF_SIZE*blockCount);
        }

        if(length > 0)
            strcat(buffer, inBuffer);
        else
            finished = 1;

    //}
    free(inBuffer);
    *len = totalLength;
    return buffer;
}

int writeSocket(int sock, char * buffer, int len){
    int sent = 0, finished = 0, totalSent = 0;

    while(!finished){
        sent = send(sock, buffer + totalSent, len - totalSent, 0);
        totalSent+=sent;

        if(sent == 0)
            finished = 1;
    }

    return totalSent;
}

int prepareSelect(int cliSock, connectionADT connection, fd_set * fdset){
    int nfds = cliSock;

    FD_ZERO( fdset );
    FD_SET( cliSock, fdset );
    for(int i = 0; i < connection->servCount ; i++){
        if(connection->servSocks[i] > nfds)
            nfds = connection->servSocks[i];
        FD_SET( connection->servSocks[i], fdset );
    }

    return nfds + 1;
}

int getservSocks(connectionADT connection, char * buffer, int length){
    char *hostName = parseRequest(buffer, length);
    if(hostName == NULL)
        return -1;
    struct sockaddr_in *serv_addr = findIp(hostName);
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
