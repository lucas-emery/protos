#include <buffer.h>
#include <sys/socket.h>
#include "transformation.h"
#include <stm.h>
#include <selector.h>
#include <response.h>
#include <metrics.h>

transformation_t transformations;
int transformationCount;

enum type{
    NONE,
    CLIENT,
    ORIGIN
};

typedef struct {
    int origin_fd;
    int client_fd;
    int infd, outfd;
    buffer buff;
    struct response response;
    struct response_parser parser;
    bool readFirst;

    buffer *rb , *wb, *tb;
    bool * respDone, *reqDone;
    uint8_t  raw_data[BUFF_SIZE];

    struct timeval time;

    struct state_machine stm;
} origin_t;

#define ORIGIN_ATTACHMENT(key) ( (origin_t *)(key)->data)

typedef struct {
    enum type type;
    int peer;
    char host[64];
    char state[32];
} table_entry_t;

extern table_entry_t table[128];

typedef enum {
    COPY,
    DONE,
    ERROR
}transform_state_t;

typedef struct {
    buffer *b, *aux;
    struct state_machine stm;
    int client_fd;

    struct timeval time;
    bool timing;
}transform_t;

static char * state_to_string(const transform_state_t st){
    switch (st) {
        case COPY:
            return "COPY";
        default:
            return "DONE";
    }
}

static unsigned
copy_r(struct selector_key *key);

static unsigned
copy_w(struct selector_key *key);

#define TRANSFORM_ATTACHMENT(key) ( (transform_t *)(key)->data)

static const struct state_definition transform_statbl[] = {
        {
                .state            = COPY,
                .on_write_ready   = copy_w,
                .on_read_ready    = copy_r
        },{
                .state            = DONE,
        },{
                .state            = ERROR,
        }
};


static const struct state_definition * transform_describe_states(void){
    return transform_statbl;
}


transform_t * transform_new(int client_fd, bool write){
    transform_t * ret = malloc(sizeof(transform_t));

    if(ret == NULL)
        return NULL;

    memset(ret, 0x00, sizeof(*ret));
    /*
    table[my_fd].type = WRITE;
    strcpy(table[origin_fd].host, "");
    strcpy(table[origin_fd].state, "CONNECTING");
    */

    ret->stm.initial = COPY;
    ret->stm.max_state = ERROR;
    ret->stm.states = transform_describe_states();
    stm_init(&ret->stm);

    ret->client_fd = client_fd;

    return ret;
}

static void transform_done(struct selector_key *key){

}

static void transform_read(struct selector_key *key) {
    struct state_machine *stm   = &TRANSFORM_ATTACHMENT(key)->stm;
    const transform_state_t st = stm_handler_read(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(ERROR == st || DONE == st) {
        transform_done(key);
    }
}

static void transform_write(struct selector_key *key) {
    struct state_machine *stm   = &TRANSFORM_ATTACHMENT(key)->stm;
    const transform_state_t st = stm_handler_write(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(ERROR == st || DONE == st) {
        transform_done(key);
    }
}

const struct fd_handler transform_handler = {
        .handle_read   = transform_read,
        .handle_write  = transform_write,
};

static unsigned
copy_r(struct selector_key *key) {
    transform_t * t = (transform_t*) key->data;
    buffer * b = t->b;
    ssize_t n, min;
    size_t size, body;

    if(t->timing) {
        logTime(TRANSFORMING,&t->time);
        t->timing = false;
    }

    uint8_t * ptr = buffer_write_ptr(b, &size);
    uint8_t * bodyPtr = buffer_read_ptr(t->aux, &body);

    min = size < body?size:body;

    memcpy(ptr, bodyPtr, min);
    buffer_write_adv(b, min);
    buffer_read_adv(t->aux,min);

    ptr = buffer_write_ptr(b, &size);
    n = read(key->fd, ptr, size);
    if(n < 0) {
        return ERROR;
    } else {
        buffer_write_adv(b, n);
    }
    selector_add_interest(key->s,t->client_fd, OP_WRITE);

    return COPY;
}

static unsigned
copy_w(struct selector_key *key) {
    transform_t * t = TRANSFORM_ATTACHMENT(key);

    size_t size;
    ssize_t n;
    buffer* b = t->b;
    uint8_t *ptr = buffer_read_ptr(b, &size);
    if(size == 0){
        selector_remove_interest(key->s, key->fd, OP_WRITE);
        return COPY;
    }

    if(!t->timing) {
        startTimer(&t->time);
        t->timing = true;
    }

    n = write(key->fd, ptr, size);
    if(n == -1) {
        return ERROR;
    } else {
        buffer_read_adv(b, n);
    }

    return COPY;
}

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


unsigned init_transform(struct selector_key *key){
    origin_t * o = ORIGIN_ATTACHMENT(key);
    int fds[] = { -1, -1, -1, -1};

    enum {
        R  = 0,
        W  = 1,
    };
    int ret = 0;

    int in [] = { -1, -1};
    int out[] = { -1, -1};
    if(pipe(in) == -1 || pipe(out) == -1) {
        return ERROR;
    }
    printf("Forking\n");
    const pid_t cmdpid = fork();
    if (cmdpid == -1) {
        return ERROR;
    } else if (cmdpid == 0) {
        close(0);
        close(1);
        close(in [W]);
        close(out[R]);
        in [W] = out[R] = -1;
        dup2(in [R], STDIN_FILENO);
        dup2(out[W], STDOUT_FILENO);

        if(-1 == execv("bin/toUpper", (char **) 0)) {
            close(in [R]);
            close(out[W]);
            ret = 1;
        }

        exit(ret);
    } else {
        close(in [R]);
        close(out[W]);
        in[R] = out[W] = -1;

        fds[2] = in[W];
        fds[3] = out[R];
    }

    transform_t * tIn = transform_new(o->client_fd, false);
    transform_t * tOut = transform_new(o->client_fd, true);


    if(tIn == NULL || tOut == NULL)
        return ERROR;

    tIn->aux = &o->buff;
    tOut->aux = &o->buff;
    tOut->b = o->rb;

    o->tb = malloc(sizeof(buffer));
    buffer_init(o->tb, BUFF_SIZE,o->raw_data);

    tIn->b = o->tb;

    o->infd = fds[2];
    o->outfd = fds[3];

    selector_register(key->s, o->infd, &transform_handler, OP_NOOP, tIn);
    selector_register(key->s, o->outfd, &transform_handler, OP_READ, tOut);
    return ret;
} //b8:o->rb
    //98:0>wb