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

#define N(x) (sizeof(x)/sizeof((x)[0]))
#define ORIGIN_ATTACHMENT(key) ( (origin_t *)(key)->data)
#define CLIENT_ATTACHMENT(key) ( (client_t *)(key)->data)

typedef enum {
    CONNECTING,
    RESPONSE_READ,
    RESPONSE_TRANSFORM,
    RESPONSE_WRITE,
    RESPONSE_DONE,
    RESPONSE_ERROR,
} origin_state_t;

typedef struct {
    int origin_fd;
    int client_fd;

    char * response;
    uint8_t length;

    struct state_machine stm;
} origin_t;

typedef struct {
    /** buffer utilizado para I/O */
    buffer                    *rb, *wb;

    /** parser */
    struct request             request;
    struct request_parser      parser;

    /** el resumen del respuesta a enviar*/
    enum socks_response_status status;

    // ¿a donde nos tenemos que conectar?
    struct sockaddr_storage   *origin_addr;
    socklen_t                 *origin_addr_len;
    int                       *origin_domain;

    const int                 *client_fd;
    int                       *origin_fd;
} request_st;

typedef struct {
    /** información del cliente */
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    /** resolución de la dirección del origin server */
    struct addrinfo              *origin_resolution;

    /** información del origin server */
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;

    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        request_st         request;
    } client;

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    buffer read_buffer, write_buffer;

} client_t;

static unsigned connected(struct selector_key *key);

static void
response_init(const unsigned state, struct selector_key *key);

//static unsigned
//response_read_close(struct selector_key *key);

static unsigned
response_read(struct selector_key *key);

static unsigned
response_write(const unsigned state, struct selector_key *key);

static const struct state_definition origin_statbl[] = {
    {
        .state            = CONNECTING,
        .on_write_ready   = connected,
    },{
        .state            = RESPONSE_READ,
        .on_arrival       = response_init,              //TODO
    //    .on_departure     = response_read_close,        //TODO
        .on_read_ready    = response_read,              //TODO
    },{
        .state            = RESPONSE_TRANSFORM,
    //    .on_block_ready   = response_transform_done,    //TODO
    },{
        .state            = RESPONSE_WRITE,
        .on_arrival       = response_write,
    },{
        .state            = RESPONSE_DONE,

    },{
        .state            = RESPONSE_ERROR,
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

    ret->origin_fd = origin_fd;
    ret->client_fd = client_fd;

    ret->stm.initial = CONNECTING;
    ret->stm.max_state = RESPONSE_ERROR;
    ret->stm.states = origin_describe_states();
    stm_init(&ret->stm);

    return ret;
}

static void origin_done(struct selector_key* key) {
    const int fds[] = {
        ORIGIN_ATTACHMENT(key)->client_fd,
        ORIGIN_ATTACHMENT(key)->origin_fd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            close(fds[i]);
        }
    }
}


static void origin_destroy(origin_t* o){

}

static void origin_read(struct selector_key *key) {
    struct state_machine *stm   = &ORIGIN_ATTACHMENT(key)->stm;
    const origin_state_t st = stm_handler_read(stm, key);

    if(RESPONSE_ERROR == st || RESPONSE_DONE == st) {
        origin_done(key);
    }
}

static void origin_write(struct selector_key *key) {
    struct state_machine *stm   = &ORIGIN_ATTACHMENT(key)->stm;
    const origin_state_t st = stm_handler_write(stm, key);

    if(RESPONSE_ERROR == st || RESPONSE_DONE == st) {
        origin_done(key);
    }
}

static void origin_block(struct selector_key *key) {
    struct state_machine *stm   = &ORIGIN_ATTACHMENT(key)->stm;
    const origin_state_t st = stm_handler_block(stm, key);

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

static unsigned connected(struct selector_key *key){
    int error = 0, len = 0;;

    if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) >= 0) {
        if(error == 0) {
            origin_t * o = (origin_t*) key->data;
            selector_notify_block(key->s,o->client_fd);
            selector_set_interest_key(key, OP_READ);
            return RESPONSE_READ;
        }
    }
    return RESPONSE_ERROR;
}

static void response_init(const unsigned state, struct selector_key *key) {
    origin_t * o = (origin_t*) key->data;
    o->response = malloc(4096);
    o->length = 0;
}

static unsigned response_read(struct selector_key *key){
    origin_t * o = (origin_t*) key->data;

    o->length = recv(key->fd, o->response, 4096, 0);
    if(o->length > 0){
        selector_set_interest_key(key, OP_NOOP);
        return RESPONSE_WRITE;
    }
    return RESPONSE_ERROR;
}

static unsigned response_write(const unsigned state, struct selector_key *key){
    printf("%p\n", key);
    origin_t * o = (origin_t*) key->data;

    printf("sending to client: %s\n", o->response);
    int sent = send(o->client_fd, o->response, o->length, 0);
    printf("sent: %d\n", sent);
    return RESPONSE_DONE;
}

void request_connect(struct selector_key *key, request_st *d) {
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
        goto finally;
    }
    struct sockaddr_in * originaddr = (struct sockaddr_in *)&CLIENT_ATTACHMENT(key)->origin_addr;
    originaddr->sin_port = htons(80);
    if (-1 == connect(*fd, (struct sockaddr_in *)&CLIENT_ATTACHMENT(key)->origin_addr,
                           CLIENT_ATTACHMENT(key)->origin_addr_len)) {
        if(errno == EINPROGRESS) {
            selector_status st = selector_set_interest_key(key, OP_NOOP);
            if(SELECTOR_SUCCESS != st) {
                error = true;
                goto finally;
            }

            origin_t * state = origin_new(*fd, key->fd);

            st = selector_register(key->s, *fd, &origin_handler, OP_WRITE, state);         //vos decime cuando puedo escribir en origin_fd (conexion terminada)

            if(SELECTOR_SUCCESS != st) {
                error = true;
                goto finally;
            }
        } else {
            status = errno_to_socks(errno);
            error = true;
            goto finally;
        }
    } else {
        // estamos conectados sin esperar... no parece posible
        // saltaríamos directamente a COPY
        abort();
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
