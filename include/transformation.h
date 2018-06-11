#ifndef PC_2018_07_TRANSFORMATION_H
#define PC_2018_07_TRANSFORMATION_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib.h"
#include <stdbool.h>
#include <selector.h>
#include <response.h>

#define TOUPPER_EXE "bin/toUpper/toUpper"
#define ECHO_EXE "bin/echo/echo"
#define ECHODEBUG_EXE "bin/echoDebug/echoDebug"

typedef enum transformation_type {
    TOUPPER,
    ECHO,
    ECHODEBUG
} transformation_type_t;

typedef struct {
    char* mediaType;
    int activated;
    transformation_type_t type;
} transformation_t;

transformation_t * listAll(int* count);
int getTransformation(const uint8_t * mediaType);
void registerTransformation(const uint8_t * mediaType, transformation_type_t type);
void unregisterTransformation(const uint8_t * mediaType);
const char * getExe(const uint8_t * mediaType);
bool isActive(const uint8_t * mediaType);
unsigned init_transform(struct selector_key *key, bool chunked, size_t content_length);
void transform_headers(struct response * response);


#endif