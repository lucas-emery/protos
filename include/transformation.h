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
#define DUPLICATE_EXE "bin/duplicate/duplicate"
#define LEET_EXE "sed"

typedef enum transformation_type {
    TOUPPER,
    ECHO,
    DUPLICATE,
    LEET,
} transformation_type_t;

typedef struct {
    char* mediaType;
    int activated;
    transformation_type_t type;
} transformation_t;

transformation_t * listAll(int* count);
char ** get_args(const uint8_t *mediaType);
int get_transformation(const uint8_t *mediaType);
void register_transformation(const uint8_t *mediaType, transformation_type_t type);
void unregister_transformation(const uint8_t *mediaType);
const char * get_exe(const uint8_t *mediaType);
bool is_active(const uint8_t *mediaType);
unsigned init_transform(struct selector_key *key, bool chunked, size_t content_length);
void transform_headers(struct response * response);

void
close_transformations();


#endif