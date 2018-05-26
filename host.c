#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "requestParser.h"
#include "message.h"

#define BUFF_SIZE 1024
#define PORT_NUMBER 9090

struct sockaddr_in* findIp(char* hostname);
void connectToOrigin(struct sockaddr_in* host, char* content, int length);

int connectToClient(){

}

int main(int argc, char const *argv[]) {
    int sockfd, newsockfd, n;
    socklen_t clilen;
    char * inBuffer = malloc(BUFF_SIZE);
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT_NUMBER);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        DieWithUserMessage("ded","ERROR on binding");
    listen(sockfd,5);

    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (newsockfd < 0)
      DieWithSystemMessage("ERROR on accept");

    bzero(inBuffer,1024);
    n = recv(newsockfd,inBuffer,BUFF_SIZE,0);

    if (n < 0)
        DieWithSystemMessage("ERROR reading from socket");

    printf("Request:\n%s",inBuffer);

    char* hostname = parseRequest(inBuffer, n);

    connectToOrigin(findIp(hostname), inBuffer, n);

    free(hostname);

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
