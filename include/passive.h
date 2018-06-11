#ifndef PC_2018_07_PASSIVE_H
#define PC_2018_07_PASSIVE_H

#include <netdb.h>
#include "selector.h"
#include "buffer.h"
#include "request.h"
#include "stm.h"


#define CLIENT_ATTACHMENT(key) ( (client_t *)(key)->data)

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

    struct timeval time;

    ssize_t bodyWritten;
    bool * respDone, * reqDone, * transDone;
    uint8_t raw_buff_a[BUFF_SIZE], raw_buff_b[BUFF_SIZE], raw_buff_aux[BUFF_SIZE];
    buffer read_buffer, write_buffer, aux_buffer;

} client_t;

/** handler del socket pasivo que atiende conexiones socksv5 */
void socks_passive_accept(struct selector_key *key);

/** libera pools internos */
void sock_pool_destroy(void);

void
local_ip_resolv();

#endif //PC_2018_07_PASSIVE_H
