#include "transformation.h"
#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>

#include <arpa/inet.h>

#include "request.h"
#include "buffer.h"

#include "stm.h"
#include "passive.h"
#include "netutils.h"
#include "response.h"

transformation_t transformations;
int transformationCount;

typedef struct {
    int client_fd;
    int size;
}transform_t;

transform_t * transform_new(int client_fd){
    transform_t * ret = malloc(sizeof(transform_t));

    if(ret == NULL)
        return NULL;

    ret->client_fd = client_fd;
    ret->size = 0;

    return ret;
}

static void transform_read(struct selector_key *key){
    transform_t * t = (transform_t*) key->data;
    char buffer[BUFF_SIZE];
    printf("reading from toUpper\n");
    ssize_t r = read(key->fd, buffer, BUFF_SIZE);
    t->size += r;
    send(t->client_fd,buffer,r,0);
}


const struct fd_handler transform_handler = {
    .handle_read   = transform_read,
};

int equals(transformation_t transformation1, transformation_t transformation2);

void activateTransformation(char* mediaType, transformation_type_t type) {
    int finished = FALSE;
    transformation new;
    new.mediaType = mediaType;
    new.type = type;
    new.activated = TRUE;

    for (size_t i = 0; i < transformationCount && !finished; i++) {

        if( equals(&new, transformations + i) ) {
            transformations[i].activated = TRUE;
            finished = TRUE;
        }
    }

    if(!finished) {
        transformationCount++;
        transformations = realloc(transformations, transformationCount * sizeof(transformation));
        transformations[transformationCount - 1] = new;
    }
}

int deactivateTransformation(char* mediaType, transformation_type_t type) {
    int finished = FALSE;
    transformation new;
    new.mediaType = mediaType;
    new.type = type;

    for (size_t i = 0; i < transformationCount && !finished; i++) {

        if( equals(&new, transformations + i) ) {
            transformations[i].activated = FALSE;
            finished = TRUE;
        }
    }

    if(!finished) {
        return -1;
    }

    return 0;
}

void execute(char* mediaType, char* body) {

    for (size_t i = 0; i < transformationCount; i++) {

        transformation t = transformations[i];

        if( strcmp(t.mediaType, mediaType) == 0 && t.activated) {
            //run
        }
    }
}

transformation_t listAll(int* count) {
    *count = transformationCount;
    return transformations;
}

int equals(transformation_t transformation1, transformation_t transformation2) {
    int mediaType = strcmp(transformation1->mediaType, transformation2->mediaType) == 0;
    int type = transformation1->type == transformation2->type;
    return mediaType && type;
}
