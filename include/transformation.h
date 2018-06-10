#ifndef PC_2018_07_TRANSFORMATION_H
#define PC_2018_07_TRANSFORMATION_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib.h"

typedef enum transformation_type {
    TOUPPER
} transformation_type_t;

typedef struct {
    char* mediaType;
    int activated;
    transformation_type_t type;
} transformation;

typedef transformation* transformation_t;

transformation_t listAll(int* count);
void activateTransformation(char* mediaType, transformation_type_t type);
int deactivateTransformation(char* mediaType, transformation_type_t type);
void execute(char* mediaType, char* body);

#endif