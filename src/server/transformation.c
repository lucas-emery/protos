#include <buffer.h>
#include <sys/socket.h>
#include "transformation.h"
#include <stm.h>
#include <selector.h>

#include <metrics.h>


#define BLOCK 5
#define EXE_COUNT 2
transformation_t ** transformations;
int transformationCount, size;

typedef struct {
    transformation_type_t type;
    char * exec;
} exe_t;

static exe_t exe_array[] = {
        {
            .type   = TOUPPER,
            .exec   = TOUPPER_EXE,
        },{
            .type   = ECHO,
            .exec   = ECHO_EXE,
        },{
            .type   = ECHODEBUG,
            .exec   = ECHODEBUG_EXE
        }
};

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
    bool * respDone, *reqDone, *transDone;
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

    bool * transDone;

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


transform_t * transform_new(int client_fd){
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

char *substring(char *string, int position, int length)
{
    char *pointer;
    int c;

    pointer = malloc(length+1);

    if( pointer == NULL )
        exit(EXIT_FAILURE);

    for( c = 0 ; c < length ; c++ )
        *(pointer+c) = *((string+position-1)+c);

    *(pointer+c) = '\0';

    return pointer;
}

void
transform_headers(struct response response) {
    if(response.body_length != 0) {
        int j, header_length = response.header_length;
        uint8_t *headers = response.headers;
        char content[header_length];
        int before, i;
        bool found = false;

        memcpy(content, strstr(headers, "Content-Length"), header_length);

        for (j = 0; content[j] != '\r'; j++);

        content[j] = 0;

        char *f, *e;
        int length;

        for (i = 0; headers[i] != '\n' ; i++);

        length = strlen(headers);

        f = substring(headers, 1, i - 1 );
        e = substring(headers, i, length-i+1);

        strcpy(headers, "");
        strcat(headers, f);
        free(f);
        strcat(headers, "\r\nTransfer-Encoding: chunked");
        strcat(headers, e);
        free(e);

        for (; before < header_length && !found; before++) {
            char c = headers[before];
            if(c == 'C') {
                if(strncmp(headers + before, content, j) == 0)
                    found = true;
            }
        }

        before--;

        memmove(headers + before - 2, headers + before + strlen(content) + 2, strlen(headers) - before - strlen(content));
    }
}

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
    if(n == 0)
        *t->transDone = true;
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

int equals(transformation_t * transformation1, transformation_t * transformation2);

void registerTransformation(const char* mediaType, transformation_type_t type) {
    int index = getTransformation(mediaType);
    if(index < 0){
        transformation_t * new = malloc(sizeof(transformation_t));
        new->mediaType = strdup(mediaType);
        new->type = type;

        if(transformationCount == size){
            transformations = realloc(transformations, (BLOCK + transformationCount) * sizeof(transformation_t) );
            size += BLOCK;
        }

        transformations[transformationCount++] = new;
    } else {
        transformations[index]->type = type;
    }
}

void unregisterTransformation(const char* mediaType) {
    int index = getTransformation(mediaType);
    if(index < 0){
        return;
    }

    free(transformations[index]->mediaType);
    free(transformations[index]);

    for (int i = index; i < transformationCount - 1; ++i) {
        transformations[i] = transformations[i+1];
    }

    transformations[transformationCount--] = NULL;
}

int getTransformation(const char* mediaType){
    for (int i = 0; i < transformationCount; ++i) {
        if(strcmp(transformations[i]->mediaType, mediaType) == 0){
            return i;
        }
    }
    return -1;
}

const char * getExe(const char* mediaType){
    int index = getTransformation(mediaType);

    if(index < 0)
        return NULL;

    transformation_type_t type = transformations[index]->type;

    for (int i = 0; i < EXE_COUNT; ++i) {
        if(type == exe_array[i].type)
            return exe_array[i].exec;
    }

    return NULL;

}

bool isActive(const char * mediaType){
    return getTransformation(mediaType) != -1;
}

transformation_t * listAll(int* count) {
    *count = transformationCount;
    return transformations;
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

        if(-1 == execv(getExe(o->response.mediaType), (char **) 0)) {
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

    transform_t * tIn = transform_new(o->client_fd);
    transform_t * tOut = transform_new(o->client_fd);

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

    tIn->transDone = o->transDone;
    tOut->transDone = o->transDone;

    selector_register(key->s, o->infd, &transform_handler, OP_NOOP, tIn);
    selector_register(key->s, o->outfd, &transform_handler, OP_READ, tOut);
    return ret;
}

