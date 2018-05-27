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

#define BUFF_SIZE 1024
#define PORT_NUMBER 8085

typedef enum method {CONNECT, GET, POST, HEAD} method_t;

typedef struct {
    char * hostname;
    method_t method;
} request_t;

struct sockaddr_in* findIp(char* hostname);
void connectToOrigin(struct sockaddr_in* host, char* content, int length);

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

int main(int argc, char const *argv[]) {

    int mSock, cliSock, servSock, n;
    socklen_t clilen;
    char * inBuffer = malloc(BUFF_SIZE);

    fflush(stdout);

    struct sockaddr_in *my_addr = malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *cli_addr = malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *serv_addr = malloc(sizeof(struct sockaddr_in));

    mSock = socket(AF_INET, SOCK_STREAM,0);
    bzero((char *) my_addr, sizeof(struct sockaddr_in));
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(PORT_NUMBER);
    serv_addr->sin_addr.s_addr = INADDR_ANY;

    int enable = 1;
    if (setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        DieWithSystemMessage("setsockopt(SO_REUSEADDR) failed");


    if (bind(mSock, serv_addr, sizeof(struct sockaddr_in)) < 0)
        DieWithUserMessage("ded","ERROR on binding");
    listen(mSock,5);

    clilen = sizeof(struct sockaddr_in);
    cliSock = accept(mSock, cli_addr, &clilen);

    if (cliSock < 0)
      DieWithSystemMessage("ERROR on accept");

    bzero(inBuffer,1024);
    n = recv(cliSock,inBuffer,BUFF_SIZE,0);

    if (n < 0)
        DieWithSystemMessage("ERROR reading from socket");

    printf("Request:\n%s",inBuffer);

    char* hostname = parseRequest(inBuffer, n);
    serv_addr = findIp(hostname);

    if((servSock = connectToServer(serv_addr)) >= 0){
        char connectedMessage[128];
        char * okString = "HTTP/1.0 200 Connection established\r\n\r\n";
        strcpy(connectedMessage, okString);
        //send(cliSock, okString, strlen(okString),0);
    }
    send(servSock, inBuffer, n, 0);

    struct timeval timeout;
    fd_set fdset;
    int nfds = ((cliSock > servSock)?cliSock:servSock) + 1;

    timeout.tv_sec = 30;
    timeout.tv_usec = 0;

    while(1){
        FD_ZERO( &fdset );
	    FD_SET( cliSock, &fdset );
	    FD_SET( servSock, &fdset );

        int r = select( nfds, &fdset, (fd_set*) 0, (fd_set*) 0, &timeout);
        if( r == 0  )
            DieWithSystemMessage("TIMEOUT");

        int rfd, wfd;
        int aux = 0;

        bzero(inBuffer,BUFF_SIZE);
        if(FD_ISSET(cliSock, &fdset)){
            rfd = cliSock;
            wfd = servSock;
            aux = recv(rfd, inBuffer, BUFF_SIZE,0);
            if(aux > 0){
                printf("Forwarding from Client to Server: %d\n", aux);
                printf("%s\n", inBuffer);
                write(servSock, inBuffer, aux);
                //send(servSock, inBuffer, aux, 0);
            }
        } else if(FD_ISSET(servSock, &fdset)){
            rfd = servSock;
            wfd = cliSock;
            aux = recv(rfd, inBuffer, BUFF_SIZE,0);
            if(aux > 0){
                printf("Forwarding from Server to Client: %d\n", aux);
                printf("%s\n", inBuffer);
                write(cliSock, inBuffer, aux);
                //send(servSock, inBuffer, aux, 0);
            }
        }
        /*
        bzero(inBuffer,BUFF_SIZE);
        int aux = 0;
        aux = recv(rfd, inBuffer, BUFF_SIZE,0);
        printf("%d", aux);
        if(aux > 0)
            send(servSock, inBuffer, aux, 0);
            */
    }

    bzero(inBuffer,1024);
    char aux[2048];
    bzero(aux, 2048);
    n = recv(cliSock,aux,BUFF_SIZE,0);

    if (n < 0)
        DieWithSystemMessage("ERROR reading from socket");

    printf(aux);
    //connectToOrigin(findIp(hostname), inBuffer, n);



    free(hostname);

    close(mSock);
    close(cliSock);
    close(servSock);

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
