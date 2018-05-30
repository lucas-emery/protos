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

struct chunk {
	uint8_t chunkValueType;
	uint8_t chunkValueSpecification;
};

struct chunk generateChunk(uint8_t valueType, uint8_t valueSpecification);

int getParams(int cantParams, char const *params[], char buffer[]);
void parseResponse(char * buffer);

#endif