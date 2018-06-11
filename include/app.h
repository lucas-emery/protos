#ifndef APP_H_
#define APP_H_

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
#define PASSWORD_SIZE 8
#define METRIC_SIZE 64

int getParams(int cantParams, char const *params[], char buffer[], char * ip, int * port);
void parseResponse(char * buffer, int requests);
void DieWithSystemMessage(const char *msg);
void DieWithUserMessage(const char *msg, const char *detail);
int isMediaType(const char * param);
void getPassword(char * password);
char * hash(unsigned char *str);

#endif