#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum method {
    CONNECT,
    GET,
    POST,
    HEAD
} method_t;

typedef struct request_t request_t;

struct request_t{
    char * hostname;
    method_t method;
};

void parseRequest(const char* inBuffer, int n, request_t* request);
