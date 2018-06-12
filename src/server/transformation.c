#include <buffer.h>
#include <sys/socket.h>
#include "transformation.h"
#include <stm.h>
#include <response.h>
#include <selector.h>
#include <metrics.h>
#include <transformation.h>
#include "origin.h"
#include "utils.h"


#define BLOCK 5
#define EXE_COUNT 4

transformation_t ** transformations;
int transformationCount, size;

char *leetArgv[] = {"sed", "-e", "s/a/4/g", "-e", "s/e/3/g", "-e", "s/i/1/g", "-e", "s/o/0/g", "-e", "s/5/-/g", NULL};


typedef struct {
    transformation_type_t type;
    char * exec;
    char ** params;
} exe_t;

static exe_t exe_array[] = {
        {
            .type   = TOUPPER,
            .exec   = TOUPPER_EXE,
            .params = NULL,
        },{
            .type   = ECHO,
            .exec   = ECHO_EXE,
            .params = NULL,
        },{
            .type   = ECHODEBUG,
            .exec   = ECHODEBUG_EXE,
            .params = NULL,
        },{
            .type   = LEET,
            .exec   = LEET_EXE,
            .params = leetArgv,
        }
};

enum type{
    NONE,
    CLIENT,
    ORIGIN
};

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
    enum type type;

    bool chunked;
    size_t content_length;
    bool *respDone, *reqDone, *transDone;

    uint8_t  raw_data[BUFF_SIZE];

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
clear_w(struct selector_key *key);

static unsigned
copy_r(struct selector_key *key);

static unsigned
copy_w(struct selector_key *key);

#define TRANSFORM_ATTACHMENT(key) ( (transform_t *)(key)->data)

static const struct state_definition transform_statbl[] = {
        {
                .state            = COPY,
                .on_write_ready   = copy_w,
                .on_read_ready    = copy_r,
        },{
                .state            = DONE,
                .on_write_ready   = clear_w,
        },{
                .state            = ERROR,
                .on_write_ready   = clear_w,
        }
};


static const struct state_definition * transform_describe_states(void){
    return transform_statbl;
}


transform_t * transform_new(int client_fd){
    transform_t * ret = calloc(1, sizeof(transform_t));

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
    selector_set_interest_key(key, OP_NOOP);
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

static void transform_close(struct selector_key *key) {
    transform_t * t = TRANSFORM_ATTACHMENT(key);

    if(t->type == ORIGIN) {
        free(t->b);
    } else {
        free(t->aux);
    }
    free(t);
}

const struct fd_handler transform_handler = {
        .handle_read   = transform_read,
        .handle_write  = transform_write,
        .handle_close  = transform_close,
};

void
transform_headers(struct response * response) {
    int i, header_length = response->header_length;
    char * headers = (char *)response->headers;

    char * cont_length_header = strcasestr(headers, "Content-Length");

    if(cont_length_header != NULL) {
        for (i = 0; cont_length_header[i] != '\n' ; i++);
        i++;

        memmove(cont_length_header, &cont_length_header[i], (size_t)header_length);

        char * end = strstr(headers, "\r\n\r\n");
        char * chunked = "\r\nTransfer-Encoding: chunked\r\n\r\n";
        size_t length = strlen(chunked);
        memcpy(end, chunked, length);
        response->header_length = (int)((void *)end - (void *)headers + length);
    }
}

static unsigned clear_w(struct selector_key *key) {
    transform_t * t = TRANSFORM_ATTACHMENT(key);
    selector_remove_interest_key(key, OP_WRITE);
    return t->stm.current->state;
}

ssize_t max_chunk_length(size_t size) {
    size_t metadata_length = 5;     // \r\n\r\n
    size_t aux = size;
    do {
        metadata_length++;
        aux /= 16;
    } while(aux != 0);
    return size - metadata_length;
}

char * size_to_hexstring(size_t size) {
    char * buf;
    size_t strlength = 1;
    size_t aux = size;
    do {
        strlength++;
        aux /= 16;
    } while(aux != 0);

    buf = malloc(strlength);
    sprintf(buf, "%zx", size);
    return buf;
}

static unsigned
copy_r(struct selector_key *key) {
    transform_t * t = (transform_t*) key->data;
    buffer * b = t->b;
    buffer * aux = t->aux;
    ssize_t n, max_length;
    size_t size, aux_size, min, length;
    char * hexstring;

    if(t->timing) {
        logTime(TRANSFORMING,&t->time);
        t->timing = false;
    }

    uint8_t * ptr = buffer_write_ptr(b, &size);
    uint8_t * aux_ptr = buffer_write_ptr(aux, &aux_size);

    min = size < aux_size ? size : aux_size;

    max_length = max_chunk_length(min);

    if(max_length <= 0) {
        selector_add_interest(key->s, t->client_fd, OP_WRITE);
        return COPY;
    }

    n = read(key->fd, aux_ptr, (size_t)max_length);
    if(n < 0) {
        *t->reqDone = true;
        *t->respDone = true;
        *t->transDone = true;
        selector_add_interest(key->s, t->client_fd, OP_WRITE);
        return ERROR;
    } else if(n == 0 && max_length != 0) {
        *t->transDone = true;
    } else {
        buffer_write_adv(aux, n);
    }

    hexstring = size_to_hexstring((size_t)n);
    length = strlen(hexstring);
    memcpy(ptr, hexstring, length);
    free(hexstring);
    buffer_write_adv(b, length);

    ptr = buffer_write_ptr(b, &size);
    memcpy(ptr, "\r\n", 2);
    buffer_write_adv(b, 2);

    ptr = buffer_write_ptr(b, &size);
    memcpy(ptr, aux_ptr, (size_t)n);
    buffer_read_adv(aux, n);
    buffer_write_adv(b, n);

    ptr = buffer_write_ptr(b, &size);
    memcpy(ptr, "\r\n", 2);
    buffer_write_adv(b, 2);


    selector_add_interest(key->s,t->client_fd, OP_WRITE);

    return *t->transDone ? DONE : COPY;
}

bool
get_chunk_length(uint8_t * data, size_t size, size_t * length, size_t * offset) {
    bool valid = false;
    size_t start, i;
    for(start = 0; (data[start] == '\r' || data[start] == '\n') && start < size; start++);
    for (i = start; i < size; i++) {
        if(data[i] == '\n') {
            valid = true;
            i++;
            break;
        }
    }

    if(valid) {
        *length = (size_t) strtol((char*) data + start, NULL, 16);
        *offset = i;
    }
    return valid;
}

static unsigned
copy_w(struct selector_key *key) {
    transform_t * t = TRANSFORM_ATTACHMENT(key);

    size_t size, min, length, offset;
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

    if(t->chunked && t->content_length == 0) {
        if(get_chunk_length(ptr, size, &length, &offset)) {
            t->content_length = length;
            buffer_read_adv(b, offset);
            ptr = buffer_read_ptr(b, &size);

            if(length == 0) {
                return DONE;
            }
        } else {
            return COPY;
        }
    }

    min = size < t->content_length ? size : t->content_length;

    n = write(key->fd, ptr, min);
    if(n == -1) {
        *t->reqDone = true;
        *t->respDone = true;
        *t->transDone = true;
        selector_add_interest(key->s, t->client_fd, OP_WRITE);
        return ERROR;
    } else {
        buffer_read_adv(b, n);
        t->content_length -= n;
        if(!t->chunked && t->content_length == 0) {
            return DONE;
        }
    }

    return COPY;
}

void register_transformation(const uint8_t *mediaType, transformation_type_t type) {
    int index = get_transformation(mediaType);
    if(index < 0){
        transformation_t * new = calloc(1, sizeof(transformation_t));
        new->mediaType = calloc(1, strlen((char*)mediaType));
        strcpy(new->mediaType, (char*)mediaType);
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

void unregister_transformation(const uint8_t *mediaType) {
    int index = get_transformation(mediaType);
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

int get_transformation(const uint8_t *mediaType){
    char* charset = strchr((char*) mediaType, ';');
    size_t charset_length = charset ==  NULL ? 0 : strlen(charset);

    for (int i = 0; i < transformationCount; ++i) {
        char* current = transformations[i]->mediaType;
        char* current_charset = strchr(current, ';');

        if(current_charset != NULL && strcmp(current, (char*)mediaType) == 0){
            return i;

        } else if(current_charset == NULL &&
                  strncmp(current, (char*) mediaType, strlen((char*)mediaType) - charset_length) == 0) {
                return i;
        }
    }
    return -1;
}

const char * get_exe(const uint8_t *mediaType){
    int index = get_transformation(mediaType);

    if(index < 0)
        return NULL;

    transformation_type_t type = transformations[index]->type;

    for (int i = 0; i < EXE_COUNT; ++i) {
        if(type == exe_array[i].type)
            return exe_array[i].exec;
    }

    return NULL;
}

char ** get_args(const uint8_t *mediaType){
    int index = get_transformation(mediaType);

    if(index < 0)
        return NULL;

    transformation_type_t type = transformations[index]->type;

    for (int i = 0; i < EXE_COUNT; ++i) {
        if(type == exe_array[i].type)
            return exe_array[i].params;
    }

    return NULL;
}

bool is_active(const uint8_t *mediaType){
    return get_transformation(mediaType) != -1;
}

transformation_t * listAll(int* count) {
    *count = transformationCount;
    return *transformations;
}

void
close_transformations() {
    for (int i = 0; i < transformationCount; ++i) {
        free(transformations[i]->mediaType);
        free(transformations[i]);
    }
    free(transformations);
}


unsigned
init_transform(struct selector_key *key, bool chunked, size_t content_length) {
    origin_t * o = ORIGIN_ATTACHMENT(key);
    int fds[] = { -1, -1, -1, -1};

    enum {
        R  = 0,
        W  = 1,
    };
    unsigned ret = 0;

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

        if(-1 == execvp(get_exe(o->response.mediaType), get_args(o->response.mediaType))) {
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

    tIn->type = ORIGIN;
    tOut->type = CLIENT;

    o->tb = malloc(sizeof(buffer));
    buffer_init(o->tb, BUFF_SIZE,o->raw_data);

    tIn->b = o->tb;
    tOut->b = o->rb;

    tIn->aux = &o->buff;
    tOut->aux = malloc(sizeof(buffer));
    buffer_init(tOut->aux, BUFF_SIZE, tOut->raw_data);

    o->infd = fds[2];
    o->outfd = fds[3];

    tIn->reqDone = o->reqDone;
    tOut->reqDone = o->reqDone;
    tIn->respDone = o->respDone;
    tOut->respDone = o->respDone;
    tIn->transDone = o->transDone;
    tOut->transDone = o->transDone;

    tIn->chunked = chunked;
    tOut->chunked = chunked;
    if(chunked) {
        tIn->content_length = 0;
        tOut->content_length = 0;
    } else {
        tIn->content_length = content_length;
        tOut->content_length = content_length;
    }

    selector_register(key->s, o->infd, &transform_handler, OP_NOOP, tIn);
    selector_register(key->s, o->outfd, &transform_handler, OP_READ, tOut);
    return ret;
}

