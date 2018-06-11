#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <unistd.h>  // close
#include <pthread.h>

#include <arpa/inet.h>
#include <metrics.h>
#include <transformation.h>
#include <passive.h>

#include "request.h"
#include "passive.h"
#include "netutils.h"
#include "log.h"
#include "origin.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

void startTime();
void printDeltaTime();

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
    CONNECTING,
    HEADERS,
    TRANSFORM,
    COPY,
    RESPONSE_DONE,
    RESPONSE_ERROR,
} origin_state_t;

static void clear_interests(const unsigned state, struct selector_key *key);

static unsigned connected(struct selector_key *key);

static void headers_init(const unsigned state, struct selector_key *key);

static void headers_flush(const unsigned state, struct selector_key *key);

static unsigned flush_body(struct selector_key *key);

static unsigned headers_read(struct selector_key *key);

static unsigned transform(struct selector_key *key);

static unsigned copy_r(struct selector_key *key);

static unsigned copy_w(struct selector_key *key);

static void destroy(const unsigned state, struct selector_key *key);

static char * state_to_string(const origin_state_t st){
    switch (st) {
        case CONNECTING:
            return "CONNECTING";
        case HEADERS:
            return "HEADERS";
        case TRANSFORM:
            return "TRANSFORM";
        case COPY:
            return "COPY";
        default:
            return "DONE";
    }
}

static const struct state_definition origin_statbl[] = {
    {
        .state            = CONNECTING,
        .on_write_ready   = connected,
    },{
        .state            = HEADERS,
        .on_arrival       = headers_init,              //TODO
        .on_read_ready    = headers_read,              //TODO
        .on_departure     = headers_flush
    },{
        .on_read_ready    = transform,
        .state            = TRANSFORM,
    //    .on_block_ready   = response_transform_done,    //TODO
    },{
        .on_write_ready   = copy_w,
        .on_read_ready    = copy_r,
        .on_block_ready   = flush_body,
        .state            = COPY,
    },{
        .state            = RESPONSE_DONE,
        .on_arrival       = clear_interests,
    },{
        .state            = RESPONSE_ERROR,
        .on_arrival       = clear_interests,
    }
};


static const struct state_definition * origin_describe_states(void){
    return origin_statbl;
}

static origin_t * origin_new(int origin_fd, int client_fd) {
    origin_t * ret;

    ret = malloc(sizeof(*ret));

    if(ret == NULL) {
        return NULL;
    }
    memset(ret, 0x00, sizeof(*ret));

    table[origin_fd].type = ORIGIN;
    table[origin_fd].peer = client_fd;
    table[client_fd].peer = origin_fd;
    strcpy(table[origin_fd].host, "");
    strcpy(table[origin_fd].state, "CONNECTING");

    ret->origin_fd = origin_fd;
    ret->client_fd = client_fd;
    ret->infd = -1;
    ret->outfd = -1;

    ret->stm.initial = CONNECTING;
    ret->stm.max_state = RESPONSE_ERROR;
    ret->stm.states = origin_describe_states();
    stm_init(&ret->stm);

    return ret;
}

static void origin_done(struct selector_key* key) {
    register_stop(ORIGIN_ATTACHMENT(key)->client_fd);
    /*const int fds[] = {
        ORIGIN_ATTACHMENT(key)->client_fd,
        ORIGIN_ATTACHMENT(key)->origin_fd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            table[key->fd].type = NONE;
            strcpy(table[key->fd].state, "");
            close(fds[i]);
        }
    }*/
}


static void origin_destroy(origin_t* o){
    if(o != NULL) {
        free(o->response.headers);
        free(o->response.mediaType);
        free(o);
    }
}

static void origin_read(struct selector_key *key) {
    struct state_machine *stm   = &ORIGIN_ATTACHMENT(key)->stm;
    const origin_state_t st = stm_handler_read(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(RESPONSE_ERROR == st || RESPONSE_DONE == st) {
        origin_done(key);
    }
}

static void origin_write(struct selector_key *key) {
    struct state_machine *stm   = &ORIGIN_ATTACHMENT(key)->stm;
    const origin_state_t st = stm_handler_write(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(RESPONSE_ERROR == st || RESPONSE_DONE == st) {
        origin_done(key);
    }
}

static void origin_block(struct selector_key *key) {
    struct state_machine *stm   = &ORIGIN_ATTACHMENT(key)->stm;
    const origin_state_t st = stm_handler_block(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(RESPONSE_ERROR == st || RESPONSE_DONE == st) {
        origin_done(key);
    }
}

static void origin_close(struct selector_key *key) {
    origin_destroy(ORIGIN_ATTACHMENT(key));
}

static const struct fd_handler origin_handler = {
    .handle_read   = origin_read,
    .handle_write  = origin_write,
    .handle_close  = origin_close,
    .handle_block  = origin_block,
};

static void clear_interests(const unsigned state, struct selector_key *key) {
    selector_set_interest_key(key, OP_NOOP);
}

static unsigned connected(struct selector_key *key){
    int error = 0;
    socklen_t len = 0;

    if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) >= 0) {
        if(error == 0) {
            logTime(CONN, &ORIGIN_ATTACHMENT(key)->time);
            return COPY;
        }
    }
    //TODO Report error to client instead of just closing the connection (aca o en origin_done?)
    return RESPONSE_ERROR;
}

static void headers_init(const unsigned state, struct selector_key *key) {
    selector_set_interest(key->s,key->fd, OP_READ);
    origin_t * o = (origin_t*) key->data;
    o->parser.response = &o->response;
    response_parser_init(&o->parser);
    buffer_init(&o->buff, BUFF_SIZE, malloc(BUFF_SIZE));
    o->readFirst = false;
}

static unsigned headers_read(struct selector_key *key){
    origin_t * o = (origin_t*) key->data;
    size_t size = BUFF_SIZE;
    uint8_t * ptr = buffer_write_ptr(&o->buff, &size);
    ssize_t read = recv(o->origin_fd, ptr, size, 0);

    bool error;

    if(read > 0){
        buffer_write_adv(&o->buff, read);
        int s = response_consume(&o->buff, &o->parser, &error);
        if(response_is_done(s, 0)) {
            register_status_code(o->client_fd, o->response.status_code);
            bool transform = is_active(o->response.mediaType);
            if (transform) {
                transform_headers(&o->response);
                init_transform(key, o->response.chunked, o->response.body_length);
                selector_remove_interest(key->s, key->fd, OP_WRITE);
                return COPY;
            }
            *o->transDone = true;
            return COPY;
        }
    }
    return HEADERS;
}

static void headers_flush(const unsigned state, struct selector_key *key){
    origin_t * o = (origin_t*) key->data;
    buffer* b    = o->rb;
    size_t size;
    uint8_t *ptr = buffer_write_ptr(b, &size);

//    size_t aux = strlen("Proxy-Connection: Close") + 2;

    if(size > o->response.header_length){
        for (size_t i = 0; i < o->response.header_length; i++) {
            ptr[i] = o->response.headers[i];
        }
//        strcpy((char*)(ptr + o->response.header_length - 2), "Proxy-Connection: Close\r\n\r\n");
    }
    buffer_write_adv(b, o->response.header_length );

    selector_set_interest(key->s, o->client_fd, OP_WRITE);
    selector_remove_interest(key->s, o->origin_fd, OP_READ);
    selector_notify_block(key->s, o->origin_fd);

    response_close(&o->parser);
}

static unsigned transform(struct selector_key *key){

        return RESPONSE_DONE;
}

void request_connect(struct selector_key *key, request_st *d) {
    client_t * s = CLIENT_ATTACHMENT(key);
    bool error                  = false;
    // da legibilidad
    enum socks_response_status status =  d->status;
    int *fd                           =  d->origin_fd;

    *fd = socket(CLIENT_ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);
    if (*fd == -1) {
        error = true;
        goto finally;
    }
    if (selector_fd_set_nio(*fd) == -1) {
        error = true;
        goto finally;
    }
    if (-1 == connect(*fd, (struct sockaddr*) &CLIENT_ATTACHMENT(key)->origin_addr,
                           CLIENT_ATTACHMENT(key)->origin_addr_len)) {
        if(errno == EINPROGRESS) {

            origin_t * o = origin_new(*fd, key->fd);

            startTimer(&o->time);

            selector_status st = selector_register(key->s, *fd, &origin_handler, OP_WRITE, o);         //vos decime cuando puedo escribir en origin_fd (conexion terminada)

            if(SELECTOR_SUCCESS != st) {
                error = true;
                goto finally;
            }

            register_origin_addr(CLIENT_ATTACHMENT(key)->client_fd, &CLIENT_ATTACHMENT(key)->origin_addr);
            o->respDone = s->respDone;
            o->reqDone = s->reqDone;
            o->transDone = s->transDone;
            o->wb = &s->read_buffer;
            o->rb = &s->write_buffer;
        } else {
            status = errno_to_socks(errno);
            error = true;
            goto finally;
        }
    } else {
        // estamos conectados sin esperar... no parece posible
        // saltaríamos directamente a COPY
        error = true;
        goto finally;
    }

finally:
    if (error) {
        if (*fd != -1) {
            close(*fd);
            *fd = -1;
        }
    }

    d->status = status;
}

static void destroy(const unsigned state, struct selector_key *key){
    origin_t * o = (origin_t*) key->data;
    selector_notify_block(key->s, o->client_fd);
}

static unsigned
flush_body(struct selector_key *key) {
    origin_t * o = ORIGIN_ATTACHMENT(key);
    buffer * b = o->tb == NULL? o->rb : o->tb;
    size_t size, body, min;

    uint8_t * ptr = buffer_write_ptr(b, &size);
    uint8_t * bodyPtr = buffer_read_ptr(&o->buff, &body);

    min = size < body? size : body;

    memcpy(ptr, bodyPtr, min);
    buffer_write_adv(b, min);
    buffer_read_adv(&o->buff, min);

    if(min != body) {
        selector_notify_block(key->s, o->origin_fd);
    } else {
        selector_add_interest(key->s, o->origin_fd, OP_READ);
    }

    if(o->infd == -1 || o->outfd == -1) {
        selector_remove_interest(key->s, o->infd, OP_WRITE);
        selector_add_interest(key->s, o->client_fd, OP_WRITE);
    }else{
        selector_remove_interest(key->s,o->client_fd, OP_WRITE);
        selector_add_interest(key->s, o->infd, OP_WRITE);
    }

    if(o->response.chunked){
        *o->respDone = chunked_is_done(ptr, min);
    } else {
        *o->respDone = body_is_done(&o->parser, min); //TODO check length status
    }

    return COPY;
}

static unsigned
copy_r(struct selector_key *key) {
    origin_t * o = ORIGIN_ATTACHMENT(key);
    buffer * b = o->tb == NULL? o->rb : o->tb;
    ssize_t n;
    size_t size;

    uint8_t * ptr= buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);
    if(n < 0 || (n == 0 && size != 0 && *o->transDone)) {
        return RESPONSE_ERROR;
    } else {
        log_metric(TRAFFIC, n);
        buffer_write_adv(b, n);
    }
    if(o->infd == -1 || o->outfd == -1) {
        selector_remove_interest(key->s, o->infd, OP_WRITE);
        selector_add_interest(key->s, o->client_fd, OP_WRITE);
    }else{
        selector_remove_interest(key->s,o->client_fd,OP_WRITE);
        selector_add_interest(key->s, o->infd, OP_WRITE);
        selector_add_interest(key->s, o->outfd, OP_READ);
    }

    bool done = false;

    if(o->response.chunked){
        done = chunked_is_done(ptr, n);
    } else {
        done = body_is_done(&o->parser, n);
    }
    *o->respDone = done;
    return COPY;
}

/** escribe bytes encolados */
static unsigned
copy_w(struct selector_key *key) {
    origin_t * o = ORIGIN_ATTACHMENT(key);

    size_t size;
    ssize_t n;
    buffer* b = o->wb;
    uint8_t *ptr = buffer_read_ptr(b, &size);
    if(size == 0){
        selector_remove_interest(key->s, key->fd, OP_WRITE);
        return COPY;
    }

    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {
        return RESPONSE_ERROR;
    } else {
        log_metric(TRAFFIC, (size_t) n);
        buffer_read_adv(b, n);
    }

    if(*o->reqDone && !*o->respDone){
        return HEADERS;
    }

    if(*o->respDone && *o->reqDone)
        return RESPONSE_DONE;

    return COPY;
}


