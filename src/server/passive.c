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
#include "log.h"
#include "stm.h"
#include "passive.h"
#include "netutils.h"
#include "response.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))


struct timeval sysTime;

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

    /**
     * recibe el mensaje `request` del cliente, y lo inicia su proceso
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - REQUEST_HEADERS        mientras el mensaje no esté completo
     *   - REQUEST_RESOLV      si requiere resolver un nombre DNS
     *   - REQUEST_CONNECTING  si no require resolver DNS, y podemos iniciar
     *                         la conexión al origin server.
     *   - COPY       si determinamos que el mensaje no lo podemos
     *                         procesar (ej: no soportamos un comando)
     *   - ERROR               ante cualquier error (IO/parseo)
     */
    REQUEST_HEADERS,

    /**
     * Espera la resolución DNS
     *
     * Intereses:
     *     - OP_NOOP sobre client_fd. Espera un evento de que la tarea bloqueante
     *               terminó.
     * Transiciones:
     *     - REQUEST_CONNECTING si se logra resolución al nombre y se puede
     *                          iniciar la conexión al origin server.
     *     - COPY      en otro caso
     */
    //REQUEST_RESOLV,

    /**
     * envía la respuesta del `request' al cliente.
     *
     * Intereses:
     *   - OP_WRITE sobre client_fd
     *   - OP_NOOP  sobre origin_fd
     *
     * Transiciones:
     *   - COPY         si el request fue exitoso y tenemos que copiar el
     *                  contenido de los descriptores
     *   - ERROR        ante I/O error
     */
    COPY,
    WAITING,

    // estados terminales
    DONE,
    ERROR,
} sock_state_t;

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

    size_t bodyWritten;
    bool * respDone, * reqDone;
    uint8_t raw_buff_a[BUFF_SIZE], raw_buff_b[BUFF_SIZE], raw_buff_aux[BUFF_SIZE];
    buffer read_buffer, write_buffer, aux_buffer;

} client_t;

void startTime(){
    gettimeofday(&sysTime, NULL);
}

void printDeltaTime(){
    struct timeval stop;
    gettimeofday(&stop, NULL);
    printf("took %lu\n", stop.tv_usec - sysTime.tv_usec);
}

void request_connect(struct selector_key *key, request_st *d);

#define CLIENT_ATTACHMENT(key) ( (client_t *)(key)->data)

static void request_init(const unsigned state, struct selector_key *key);

static void request_read_done(struct selector_key *key);

static unsigned request_read(struct selector_key *key);

static unsigned request_resolv_done(struct selector_key *key);

static unsigned copy_r(struct selector_key *key);

static unsigned copy_w(struct selector_key *key);

static unsigned destroy(struct selector_key *key);

static const struct state_definition client_statbl[] = {
    {
        .state            = REQUEST_HEADERS,
        .on_arrival       = request_init,
        .on_read_ready    = request_read,
    },{
//        .state            = REQUEST_RESOLV,
//        .on_block_ready   = request_resolv_done,
//    },{
        .state            = COPY,
        .on_block_ready   = request_resolv_done,
        .on_write_ready   = copy_w,
        .on_read_ready    = copy_r,
    },{
        .state            = WAITING,
        .on_block_ready   = destroy,
    },{
        .state            = DONE,

    },{
        .state            = ERROR,
    }
};

static const struct state_definition * client_describe_states(void) {
    return client_statbl;
}

static char * state_to_string(sock_state_t state){
    switch(state){
        case REQUEST_HEADERS:
            return "REQUEST_HEADERS";
//        case REQUEST_RESOLV:
//            return "REQUEST_RESOLV";
        case COPY:
            return "COPY";
        case WAITING:
            return "WAITING";
        default:
            return "DONE/ERROR";
    }
}

static client_t * client_new(int client_fd) {
    client_t *ret;

    ret = malloc(sizeof(*ret));

    if(ret == NULL) {
        return ret;
    }
    memset(ret, 0x00, sizeof(*ret));

    table[client_fd].type = CLIENT;
    strcpy(table[client_fd].state,"REQUEST_HEADERS");

    ret->origin_fd       = -1;
    ret->client_fd       = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);

    ret->stm    .initial   = REQUEST_HEADERS;
    ret->stm    .max_state = ERROR;
    ret->stm    .states    = client_describe_states();
    stm_init(&ret->stm);

    ret->reqDone = malloc(sizeof(bool));
    ret->respDone = malloc(sizeof(bool));
    *ret->reqDone = false;
    *ret->respDone = false;
    buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);
    buffer_init(&ret->aux_buffer, N(ret->raw_buff_aux), ret->raw_buff_aux);


    return ret;
}

static unsigned request_resolv(struct selector_key * key, request_st * d);

static void client_destroy(client_t* s) {
    if(s->origin_resolution != NULL) {
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    free(s->reqDone);
    free(s->respDone);
    free(s->client.request.request.headers);
    free(s->client.request.request.host);
    free(s);
}

static void client_read(struct selector_key *key);
static void client_write(struct selector_key *key);
static void client_block(struct selector_key *key);
static void client_close(struct selector_key *key);
static const struct fd_handler client_handler = {
    .handle_read   = client_read,
    .handle_write  = client_write,
    .handle_close  = client_close,
    .handle_block  = client_block,
};


void socks_passive_accept(struct selector_key *key){
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    client_t * state = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);

    if(client == -1){
        return;
    }

    if(selector_fd_set_nio(client) == -1){
        return;
    }


    state = client_new(client);
    if(state == NULL){
        goto fail;
    }

    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    proxy_data[state->client_fd].client_addr = state->client_addr;
    gettimeofday(&proxy_data[state->client_fd].start, NULL);

    if(selector_register(key->s, client, &client_handler, OP_READ, state) != SELECTOR_SUCCESS){
        goto fail;
    }

    return;
fail:
    if(client != -1) {
        close(client);
    }
    client_destroy(state);
}

static void client_done(struct selector_key* key);

static void client_read(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sock_state_t st = stm_handler_read(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(ERROR == st || DONE == st) {
        client_done(key);
    }
}

static void client_write(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sock_state_t st = stm_handler_write(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(ERROR == st || DONE == st) {
        client_done(key);
    }
}

static void client_block(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sock_state_t st = stm_handler_block(stm, key);
    strcpy(table[key->fd].state, state_to_string(st));

    if(ERROR == st || DONE == st) {
        client_done(key);
    }
}

static void client_close(struct selector_key *key) {
    client_destroy(CLIENT_ATTACHMENT(key));
}

static void client_done(struct selector_key* key) {
    log_request(proxy_data[CLIENT_ATTACHMENT(key)->client_fd]);

    const int fds[] = {
        CLIENT_ATTACHMENT(key)->client_fd,
        CLIENT_ATTACHMENT(key)->origin_fd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            strcpy(table[key->fd].state, "");
            strcpy(table[key->fd].host, "");
            table[key->fd].type = NONE;
            close(fds[i]);
        }
    }
}

static void
request_init(const unsigned state, struct selector_key *key) {
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;
    d->rb              = &(CLIENT_ATTACHMENT(key)->read_buffer);
    d->wb              = &(CLIENT_ATTACHMENT(key)->write_buffer);
    d->parser.request  = &d->request;
    d->status          = status_general_SOCKS_server_failure;
    request_parser_init(&d->parser);
    d->client_fd       = &CLIENT_ATTACHMENT(key)->client_fd;
    d->origin_fd       = &CLIENT_ATTACHMENT(key)->origin_fd;

    d->origin_addr     = &CLIENT_ATTACHMENT(key)->origin_addr;
    d->origin_addr_len = &CLIENT_ATTACHMENT(key)->origin_addr_len;
    d->origin_domain   = &CLIENT_ATTACHMENT(key)->origin_domain;

}

static unsigned
request_read(struct selector_key *key) {
    client_t * c = CLIENT_ATTACHMENT(key);
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;
    buffer *b       = d->rb;
    buffer *aux     = &c->aux_buffer;
    unsigned  ret   = REQUEST_HEADERS;
        bool  error = false;
     uint8_t *ptr, *auxPtr;
      size_t  count, auxCount, min;
     ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    auxPtr = buffer_write_ptr(aux, &auxCount);

    min = auxCount < count ? auxCount : count;

    n = recv(key->fd, ptr, min, 0);

    memcpy(auxPtr, ptr, n);
    buffer_write_adv(b,n);
    buffer_write_adv(aux, n);

    if(n > 0) {
        request_state_t st = request_consume(aux, &d->parser, &error);
        if(d->parser.request->method == CONNECT) {
            //TODO send when write ready. Empty wb and fill with err response? Unsub from READ
            ssize_t length = send(key->fd, "HTTP/1.1 405 Method Not Allowed\r\n\r\n", strlen("HTTP/1.1 405 Method Not Allowed\r\n\r\n"), 0);
            return length >= 0 ? DONE : ERROR;
        }
        if(request_is_done( &d->parser, st, 0)) {
            proxy_data[c->client_fd].request = c->client.request.request.headers;
            strcpy(table[key->fd].host,d->request.host);
            buffer_read_ptr(aux, &auxCount);
            c->bodyWritten = auxCount;
            if(c->bodyWritten == c->client.request.request.content_length) {
                *c->reqDone = true;
            }
            ret = request_resolv(key, d);
        }
    } else {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

static void *
request_resolv_blocking(void *data) {
    struct selector_key *key = (struct selector_key *) data;
    client_t       *s   = CLIENT_ATTACHMENT(key);

    pthread_detach(pthread_self());
    s->origin_resolution = 0;
    struct addrinfo hints = {
        .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
        .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
        .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
        .ai_protocol  = 0,            /* Any protocol */
        .ai_canonname = NULL,
        .ai_addr      = NULL,
        .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%d", s->client.request.request.dest_port);

    getaddrinfo(s->client.request.request.host, buff, &hints, &s->origin_resolution);
    selector_notify_block(key->s, key->fd);

    free(data);

    return NULL;
}

static unsigned
request_resolv(struct selector_key * key, request_st * d) {
    pthread_t tid;

    //TODO analize error handling. Subbing fro WRITE??
    struct selector_key* k = malloc(sizeof(*key));
    if(k == NULL) {
        d->status = status_general_SOCKS_server_failure;
        selector_set_interest_key(key, OP_WRITE);
    } else {
        memcpy(k, key, sizeof(*k));
        if(-1 == pthread_create(&tid, 0, request_resolv_blocking, k)) {
            d->status = status_general_SOCKS_server_failure;
            selector_set_interest_key(key, OP_WRITE);
        }
    }
    return COPY;
}

static unsigned
request_resolv_done(struct selector_key *key) {
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;
    client_t *s      =  CLIENT_ATTACHMENT(key);

    if(s->origin_resolution == 0) {
        d->status = status_general_SOCKS_server_failure;
    } else {
        s->origin_domain   = s->origin_resolution->ai_family;
        s->origin_addr_len = s->origin_resolution->ai_addrlen;
        memcpy(&s->origin_addr,
                s->origin_resolution->ai_addr,
                s->origin_resolution->ai_addrlen);
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }

    //TODO use status to mark connection errors and report to client

    request_connect(key, d);
    return COPY;
}

static void
request_read_done(struct selector_key *key) {
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;
    client_t *c      =  CLIENT_ATTACHMENT(key);
    *c->reqDone = true;
    request_clean(&d->parser);
}

static unsigned
destroy(struct selector_key *key){
//    client_t * c = CLIENT_ATTACHMENT(key);
    return DONE;
}

static unsigned
copy_r(struct selector_key *key) {
    client_t * c = CLIENT_ATTACHMENT(key);

    size_t size;
    ssize_t n;
    buffer* b    = &c->read_buffer;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);
    if(n < 0 || (n == 0 && size != 0)) {
        return ERROR;
    } else {
        buffer_write_adv(b, n);
    }
    selector_add_interest(key->s,c->origin_fd, OP_WRITE);
    c->bodyWritten += n;
    if(c->bodyWritten == c->client.request.request.content_length) {
        request_read_done(key);
    }
    return COPY;
}

/** escribe bytes encolados */
static unsigned
copy_w(struct selector_key *key) {
    client_t * c = CLIENT_ATTACHMENT(key);

    size_t size;
    ssize_t n;
    buffer* b = &c->write_buffer;

    uint8_t *ptr = buffer_read_ptr(b, &size);
    if(size == 0) {
        selector_remove_interest(key->s, key->fd, OP_WRITE);
        if(*c->respDone && *c->reqDone) {
            return DONE;
        } else {
            return COPY;
        }
    }
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {
        return ERROR;
    } else {
        buffer_read_adv(b, n);
    }

    return COPY;
}

