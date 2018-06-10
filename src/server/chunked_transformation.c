#include <stdlib.h>
#include <lib.h>
#include <buffer.h>
#include <string.h>

#include "chunked_transformation.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))


//
//static chunk_state_t
//enter(const uint8_t c, struct chunk_parser* p);
//
//static enum chunk_state
//length(const uint8_t c, struct chunk_parser* p);
//
//static enum chunk_state
//body(const uint8_t c, struct chunk_parser* p);


long int
chunk_consume(char* buffer, long int read, struct chunk* chunk) {
    char length[10];
    int i;
    size_t chunk_length = 0;
    char c;
    long int min;

    if(chunk->length == 0){ // no parsee nada del chunk
        for (i = 0; buffer[i] != '\n' && i < read; i++) {
            c = buffer[i];
            if(c != '\r') {
                length[i] = c;
            }
        }

        length[i] = 0;
        chunk_length = (size_t) strtol(length, NULL, 16);
        min = min(chunk_length, read);
        chunk->body = malloc(chunk_length);
        chunk->length = chunk_length;

        memcpy(chunk->body, buffer + 6, min);

    } else{
        min = min(chunk->length - chunk->parsed, read);
        memcpy(chunk->body + chunk->parsed, buffer, min);
        chunk->parsed += min;
    }
    return min;
}



int
parse_chunks(uint8_t * buffer, size_t read, struct chunk*** chunks_read, int chunk_count) {
    long int parsed = read;
    struct chunk *current_chunk;
    int local_chunk_count = chunk_count;

    for (long int i = read; i > 0; i -= parsed) {

        if(local_chunk_count == 0 || chunks_read[local_chunk_count - 1] == NULL) {
            current_chunk = malloc(sizeof(struct chunk));
            current_chunk->body = NULL;
            current_chunk->length = 0;
            current_chunk->parsed = 0;
            local_chunk_count++;
            *chunks_read = realloc(*chunks_read, local_chunk_count * sizeof(struct chunk*));
            *chunks_read[local_chunk_count - 1] = current_chunk;
        } else {
            current_chunk = *chunks_read[local_chunk_count - 1];
        }

        parsed = chunk_consume(buffer, i, current_chunk);
        current_chunk->parsed += parsed;

        if (parsed < current_chunk->length) {
            return local_chunk_count;
        }
    }

    return local_chunk_count;
}

//bool
//chunk_is_done(struct chunk_parser *p, const chunk_state_t st, bool *errored) {
//    if(st >= chunk_error && errored != 0) {
//        *errored = true;
//    }
//    return st >= chunk_done;
//}

void
chunk_parser_close(struct chunk** chunks_read, int chunk_count) {
    for (int i = 0; i < chunk_count; ++i) {
        free(chunks_read[i]->body);
        free(chunks_read[i]);
    }
}

//static void
//chunk_reset_buffer(struct chunk_parser* p) {
//    p->i = 0;
//    bzero(p->buffer, BUFF_SIZE);
//}
//
//chunk_state_t
//chunk_parser_feed (struct chunk_parser *p, const uint8_t c) {
//    enum chunk_state next;
//
//    switch(p->state) {
//        case chunk_length:
//            next = length(c, p);
//            break;
//        case chunk_enter:
//            next = enter(c, p);
//            break;
//        case chunk_body:
//            next = body(c, p);
//            break;
//        case chunk_done:
//        case chunk_error:
//        default:
//            next = chunk_error;
//            break;
//    }
//
//    return p->state = next;
//}
//
//
//static enum chunk_state
//length(const uint8_t c, struct chunk_parser* p) {
//
//    if(c == '\r') {
//        p->buffer[p->i] = 0;
//        p->current_length = atoi(p->buffer);
//        chunk_reset_buffer(p);
//        p->buffer[p->i++] = c;
//
//        return chunk_enter;
//    } else {
//        p->buffer[p->i++];
//    }
//
//    return chunk_length;
//}
//
//static enum chunk_state
//body(const uint8_t c, struct chunk_parser* p) {
//
//    if(c == '\r') {
//        p->buffer[p->i++] = c;
//        return chunk_enter;
//    } else
//        p->current_length--;
//
//}
//
//static chunk_state_t
//enter(const uint8_t c, struct chunk_parser* p) {
//
//    switch(c){
//        case '\n':
//            p->buffer[p->i++] = c;
//            if(strcmp(p->buffer, "\r\n\r\n") == 0) {
//                chunk_reset_buffer(p);
//                return p->current_length == 0 ? chunk_done : chunk_body;
//            }
//            break;
//        case '\r':
//            p->buffer[p->i++] = c;
//            break;
//        default:
//            chunk_reset_buffer(p);
//            return p->current_length == 0 ? chunk_length : chunk_body;
//    }
//
//    return chunk_enter;
//}