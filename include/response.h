#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <netinet/in.h>

#include "buffer.h"

#define BUFF_SIZE 2048
#define TRUE 1
#define FALSE 0

struct response {
    char * response;

    char* length;
    char* mediaType;
    char* body;
    int chunked;
};

enum response_state{
    response_headers,
    response_desired_header,
    response_length,
    response_media_type,
    response_encoding,
    response_enter,
    response_body,
    response_chunk_length,

    // apartir de aca están done
    response_done,

    // y apartir de aca son considerado con error
    response_error
};

struct response_parser {
   struct response *response;
   enum response_state state;
   /** cuantos bytes tenemos que leer*/
   uint8_t n;
   /** cuantos bytes ya leimos */
   uint8_t i;
   /**buffer auxiliar*/

   uint8_t chunk_number;
   char* buffer;
};

/** inicializa el parser */
void
response_parser_init (struct response_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum response_state
response_parser_feed (struct response_parser *p, const uint8_t c);

void
response_log();

static void
response_reset_buffer(struct response_parser* p);

/**
 * Permite distinguir a quien usa parser_feed si debe seguir
 * enviando caracters o no.
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool
response_is_done(const enum response_state st, bool *errored);

void
response_close(struct response_parser *p);

/**
 * por cada elemento del buffer llama a `response_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum response_state
response_consume(buffer *b, struct response_parser *p, bool *errored);
