#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib.h"

typedef struct response response_t;

struct response {
    int length;
    char* mediaType;
    char* body;
    int chunked;
};
response_t* parseResponse(const char* inBuffer, int n);
