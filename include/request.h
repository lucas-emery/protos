#ifndef PC_2018_07_REQUEST_H
#define PC_2018_07_REQUEST_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <netinet/in.h>

#include "buffer.h"

#include "lib.h"

typedef enum request_state{
    request_method,
    request_headers,
    request_desired_header,
    request_host,
    request_enter,
    request_content_length,
    request_dest_port,

    // apartir de aca están done
    request_done,

    // y apartir de aca son considerado con error
    request_error
} request_state_t;

typedef enum {
    GET,
    POST,
    HEAD,
    DELETE,
    UNSUPPORTED
} method_t;

union socks_addr {
    char fqdn[0xff];
    struct sockaddr_in  ipv4;
    struct sockaddr_in6 ipv6;
};

struct request {
    method_t method;
    char * host;
    char* headers;
    int headers_length;
    int content_length;
    uint16_t dest_port;
};

struct request_parser {
   struct request *request;
   request_state_t state;
   /*contador auxiliar */
   uint16_t i;
   /**buffer auxiliar*/
   char* buffer;
   char * host;
};

/*
 * "...
 * 6.  Replies
 *
 * The SOCKS request information is sent by the client as soon as it has
 * established a connection to the SOCKS server, and completed the
 * authentication negotiations.  The server evaluates the request, and
 * returns a reply formed as follows:
 * ..."-- sección 6
 *
 */
enum socks_response_status {
    status_succeeded                          = 0x00,
    status_general_SOCKS_server_failure       = 0x01,
    status_connection_not_allowed_by_ruleset  = 0x02,
    status_network_unreachable                = 0x03,
    status_host_unreachable                   = 0x04,
    status_connection_refused                 = 0x05,
    status_ttl_expired                        = 0x06,
    status_command_not_supported              = 0x07,
    status_address_type_not_supported         = 0x08,
};

void
request_log();

/** inicializa el parser */
void
request_parser_init (struct request_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
request_state_t
request_parser_feed (struct request_parser *p, const uint8_t c);

/**
 * por cada elemento del buffer llama a `request_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
request_state_t
request_consume(buffer *b, struct request_parser *p, bool *errored);

/**
 * Permite distinguir a quien usa socks_hello_parser_feed si debe seguir
 * enviando caracters o no.
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool
request_is_done(struct request_parser *p, const request_state_t st, bool *errored);

enum socks_response_status
errno_to_socks(int e);

void
request_close(struct request_parser *p);

#endif