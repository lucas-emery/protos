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

typedef enum {
    GET,
    POST,
    HEAD,
    CONNECT
} method_t;

struct request {
    method_t method;
    char * host;
};

enum request_state{
   request_method,
   request_host,
   request_headers,
   request_enter,
   request_body,

   // apartir de aca están done
   request_done,

   // y apartir de aca son considerado con error
   request_error
};

struct request_parser {
   struct request *request;
   enum request_state state;
   /** cuantos bytes tenemos que leer*/
   uint8_t n;
   /** cuantos bytes ya leimos */
   uint8_t i;
   /**cuantos bytes quedan por leer de una seleccion*/
   uint8_t remaining;
   /**buffer auxiliar*/
   char* buffer;
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


 void
 request_log();

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
