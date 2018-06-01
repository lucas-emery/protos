#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>

#include "buffer.h"

typedef enum {
    GET,
    POST,
    HEAD,
    CONNECT
} method_t;

struct request {
    method_t method;
    char * body;
};

typedef enum {
   request_method,
   request_version,
   request_host,
   request_headers,
   request_body,

   // apartir de aca están done
   request_done,

   // y apartir de aca son considerado con error
   request_error
} request_state_t;

struct request_parser {
   struct request *request;
   request_state_t state;
   /** cuantos bytes tenemos que leer*/
   uint8_t n;
   /** cuantos bytes ya leimos */
   uint8_t i;
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


/** inicializa el parser */
void
request_parser_init (struct request_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state
request_parser_feed (struct request_parser *p, const uint8_t c);

/**
 * por cada elemento del buffer llama a `request_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored);

/**
 * Permite distinguir a quien usa socks_hello_parser_feed si debe seguir
 * enviando caracters o no.
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool
request_is_done(const enum request_state st, bool *errored);

void
request_close(struct request_parser *p);

/**
 * serializa en buff la una respuesta al request.
 *
 * Retorna la cantidad de bytes ocupados del buffer o -1 si no había
 * espacio suficiente.
 */
extern int
request_marshall(buffer *b,
                 const enum socks_response_status status);


/** convierte a errno en socks_response_status */
enum socks_response_status
errno_to_socks(int e);

#include <netdb.h>
#include <arpa/inet.h>

/** se encarga de la resolcuión de un request */
enum socks_response_status
cmd_resolve(struct request* request,  struct sockaddr **originaddr,
            socklen_t *originlen, int *domain);
