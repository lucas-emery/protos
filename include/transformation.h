#ifndef PC_2018_07_TRANSFORMATION_H
#define PC_2018_07_TRANSFORMATION_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib.h"
#include <stdbool.h>
#include <response.h>

#define TOUPPER_EXE "bin/toUpper"
#define ECHO_EXE "bin/echo"

typedef enum transformation_type {
    TOUPPER,
    ECHO
} transformation_type_t;

typedef struct {
    char* mediaType;
    int activated;
    transformation_type_t type;
} transformation_t;

transformation_t * listAll(int* count);
int getTransformation(const char* mediaType);
void registerTransformation(const char* mediaType, transformation_type_t type);
void unregisterTransformation(const char* mediaType);
const char * getExe(const char * mediaType);
bool isActive(const char * mediaType);
void transform_headers(struct response response);


#endif