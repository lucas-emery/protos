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

#define N(x) (sizeof(x)/sizeof((x)[0]))


struct timeval sysTime;


typedef enum {

    /**
     * recibe el mensaje `request` del cliente, y lo inicia su proceso
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - REQUEST_READ        mientras el mensaje no esté completo
     *   - REQUEST_RESOLV      si requiere resolver un nombre DNS
     *   - REQUEST_CONNECTING  si no require resolver DNS, y podemos iniciar
     *                         la conexión al origin server.
     *   - REQUEST_WRITE       si determinamos que el mensaje no lo podemos
     *                         procesar (ej: no soportamos un comando)
     *   - ERROR               ante cualquier error (IO/parseo)
     */
    REQUEST_READ,

    /**
     * Espera la resolución DNS
     *
     * Intereses:
     *     - OP_NOOP sobre client_fd. Espera un evento de que la tarea bloqueante
     *               terminó.
     * Transiciones:
     *     - REQUEST_CONNECTING si se logra resolución al nombre y se puede
     *                          iniciar la conexión al origin server.
     *     - REQUEST_WRITE      en otro caso
     */
    REQUEST_RESOLV,

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
    REQUEST_WRITE,
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

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    buffer read_buffer, write_buffer;

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

static void request_read_close(const unsigned state, struct selector_key *key);

static unsigned request_read(struct selector_key *key);

static unsigned request_resolv_done(struct selector_key *key);

static unsigned request_write(struct selector_key *key);

static unsigned destroy(struct selector_key *key);

static const struct state_definition client_statbl[] = {
    {
        .state            = REQUEST_READ,
        .on_arrival       = request_init,
        .on_departure     = request_read_close,
        .on_read_ready    = request_read,
    },{
        .state            = REQUEST_RESOLV,
        .on_block_ready   = request_resolv_done,
    },{
        .state            = REQUEST_WRITE,
        .on_block_ready   = request_write,
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

static client_t * client_new(int client_fd) {
    client_t *ret;

    ret = malloc(sizeof(*ret));

    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->origin_fd       = -1;
    ret->client_fd       = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);

    ret->stm    .initial   = REQUEST_READ;
    ret->stm    .max_state = ERROR;
    ret->stm    .states    = client_describe_states();
    stm_init(&ret->stm);

    buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

finally:
    return ret;
}

static unsigned request_resolv(struct selector_key * key, request_st * d);

static void client_destroy(client_t* s) {
    if(s->origin_resolution != NULL) {
        //freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    //free(s);
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

    if(ERROR == st || DONE == st) {
        client_done(key);
    }
}

static void client_write(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sock_state_t st = stm_handler_write(stm, key);

    if(ERROR == st || DONE == st) {
        client_done(key);
    }
}

static void client_block(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sock_state_t st = stm_handler_block(stm, key);

    if(ERROR == st || DONE == st) {
        client_done(key);
    }
}

static void client_close(struct selector_key *key) {
    client_destroy(CLIENT_ATTACHMENT(key));
}

static void client_done(struct selector_key* key) {
    const int fds[] = {
        CLIENT_ATTACHMENT(key)->client_fd,
        CLIENT_ATTACHMENT(key)->origin_fd,
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

static void
request_init(const unsigned state, struct selector_key *key) {
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

    d->rb              = &(CLIENT_ATTACHMENT(key)->read_buffer);
    d->wb              = &(CLIENT_ATTACHMENT(key)->write_buffer);
    d->parser.request  = &d->request;
    d->status          = status_general_SOCKS_server_failure;
    //d->request.dest_port = htons(80);
    request_parser_init(&d->parser);
    d->client_fd       = &CLIENT_ATTACHMENT(key)->client_fd;
    d->origin_fd       = &CLIENT_ATTACHMENT(key)->origin_fd;

    d->origin_addr     = &CLIENT_ATTACHMENT(key)->origin_addr;
    d->origin_addr_len = &CLIENT_ATTACHMENT(key)->origin_addr_len;
    d->origin_domain   = &CLIENT_ATTACHMENT(key)->origin_domain;
}

static unsigned
request_read(struct selector_key *key) {
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

      buffer *b     = d->rb;
    unsigned  ret   = REQUEST_READ;
        bool  error = false;
     uint8_t *ptr;
      size_t  count;
     ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(b, n);
        int st = request_consume(b, &d->parser, &error);
        // if(d->parser.request->method == CONNECT) {
        //     int length = send(key->fd, "HTTP/1.1 405 Method Not Allowed\r\n\r\n", strlen("HTTP/1.1 405 Method Not Allowed\r\n\r\n"), 0);
        //     return ERROR;
        // }
        if(error == true) {
            int length = send(key->fd, "HTTP/1.1 405 Method Not Allowed\r\n\r\n", strlen("HTTP/1.1 405 Method Not Allowed\r\n\r\n"), 0);
            return ERROR;
        }
        if(request_is_done( &d->parser, st, 0)) {
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

    getaddrinfo(s->client.request.request.host, "http", &hints, &s->origin_resolution);

    selector_notify_block(key->s, key->fd);

    free(data);

    return NULL;
}

static unsigned
request_resolv(struct selector_key * key, request_st * d) {
    unsigned ret;
    pthread_t tid;

    struct selector_key* k = malloc(sizeof(*key));
    if(k == NULL) {
        ret       = REQUEST_WRITE;
        d->status = status_general_SOCKS_server_failure;
        selector_set_interest_key(key, OP_WRITE);
    } else {
        memcpy(k, key, sizeof(*k));
        if(-1 == pthread_create(&tid, 0,
                        request_resolv_blocking, k)) {
            ret       = REQUEST_WRITE;
            d->status = status_general_SOCKS_server_failure;
            selector_set_interest_key(key, OP_WRITE);
        } else{
            ret = REQUEST_RESOLV;
            selector_set_interest_key(key, OP_NOOP);
        }
    }
    return ret;
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
    request_connect(key, d);
    selector_set_interest_key(key, OP_NOOP);
    return REQUEST_WRITE;
}

static void
request_read_close(const unsigned state, struct selector_key *key) {
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

    request_close(&d->parser);
}

static unsigned
request_write(struct selector_key *key) {
    request_st * d = &CLIENT_ATTACHMENT(key)->client.request;
    client_t * s = CLIENT_ATTACHMENT(key);
    int length;


    int n = send(s->origin_fd, d->request.headers_before_host,
        d->request.headers_before_host_length, MSG_NOSIGNAL);

    if(n == -1) {
        return ERROR;
    } else if(n != d->request.headers_before_host_length) {
        d->request.headers_before_host_length -= n;
        return REQUEST_WRITE;
    }

    uint8_t* ptr = buffer_read_ptr(d->rb, &length);
    n = send(s->origin_fd, ptr, length, MSG_NOSIGNAL);

    if(n == -1) {
        return ERROR;
    } else if(n != length) {
        return REQUEST_WRITE;
    }

    //log_request(d->status, (const struct sockaddr *)&CLIENT_ATTACHMENT(key)->client_addr,
    //                       (const struct sockaddr *)&CLIENT_ATTACHMENT(key)->origin_addr);
    return WAITING;
}

static unsigned
destroy(struct selector_key *key){
    client_t * c = CLIENT_ATTACHMENT(key);
    printf("killing client\n");
    selector_unregister_fd(key->s, key->fd);
    close(key->fd);
    return DONE;
}
