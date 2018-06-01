#ifndef SERVERSCTP_H_
#define SERVERSCTP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

#define MAX_BUFFER 1024
#define MY_PORT_NUM 9090
 
void parseRequest(char * buffer, char * bufferRta);
void getMetric(char type, char * metric);
int applyFilter(char type);
void DieWithSystemMessage(const char *msg);
void DieWithUserMessage(const char *msg, const char *detail);

#endif