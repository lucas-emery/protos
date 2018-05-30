#include "transformation.h"

transformation_t transformations;
int transformationCount;

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