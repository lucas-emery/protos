#include "response.h"

static enum response_state
headers(const uint8_t c, struct response_parser* p);

static enum response_state
desiredHeader(const uint8_t c, struct response_parser* p);

static enum response_state
length(const uint8_t c, struct response_parser* p);

static enum response_state
mediaType(const uint8_t c, struct response_parser* p);

static enum response_state
encoding(const uint8_t c, struct response_parser* p);

static enum response_state
enter(const uint8_t c, struct response_parser* p);

static enum response_state
body(const uint8_t c, struct response_parser* p);

static enum response_state
chunk_length(const uint8_t c, struct response_parser* p);

extern enum response_state
response_consume(buffer *b, struct response_parser *p, bool *errored) {
    enum response_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       st = response_parser_feed(p, c);
       if(response_is_done(st, errored)) {
          break;
       }
    }

    return st;
}

void
response_parser_init (struct response_parser *p) {
    p->state = response_headers;
    memset(p->response, 0, sizeof(*(p->response)));
    p->response->response = malloc(BUFF_SIZE);
    p->response->length = malloc(BUFF_SIZE);
    p->response->mediaType = malloc(BUFF_SIZE);
    p->response->chunked = FALSE;
    p->response->body = malloc(BUFF_SIZE);
    p->buffer = malloc(BUFF_SIZE);
    p->chunk_number = FALSE;
}

void
response_log() {

}

static void
response_reset_buffer(struct response_parser* p) {
    p->i = 0;
    bzero(p->buffer, BUFF_SIZE);
}

bool
response_is_done(const enum response_state st, bool *errored) {
    if(st >= response_error && errored != 0) {
        *errored = true;
    }
    return st >= response_done;
}

void
response_close(struct response_parser *p) {
    free(p->buffer);
}

enum response_state
response_parser_feed (struct response_parser *p, const uint8_t c) {
    enum response_state next;

    switch(p->state) {
        case response_headers:
            next = headers(c, p);
            break;
        case response_desired_header:
            next = desiredHeader(c, p);
            break;
        case response_length:
            next = length(c, p);
            break;
        case response_media_type:
            next = mediaType(c, p);
            break;
        case response_encoding:
            next = encoding(c, p);
            break;
        case response_body:
            next = body(c, p);
            break;
        case response_enter:
            next = enter(c, p);
            break;
        case response_chunk_length:
            next = chunk_length(c, p);
            break;
        case response_done:
        case response_error:
        default:
            next = response_error;
            break;
    }

    return p->state = next;
}

static enum response_state
headers(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_headers;

    if(c == '\r') {
        response_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = response_enter;
    }

    return next;
}

static enum response_state
enter(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_enter;

    switch(c){
        case '\n':
            p->buffer[p->i++] = c;
            if(strcmp(p->buffer, "\r\n\r\n") == 0) {
                p->body_count = 0;
                response_reset_buffer(p);
                if(p->response->chunked) {
                    if(p->response->body[0] == 0)
                        next = response_chunk_length;
                    else
                        next = response_done;
                }
                else
                    next = response_body;
            }
        break;
        case '\r':
            p->buffer[p->i++] = c;
        break;
        default:
            if(c == 'T' || c == 'C') {
                response_reset_buffer(p);
                p->buffer[p->i++] = c;
                next = response_desired_header;
            } else if(p->response->chunked) {
                if(p->chunk_number) {
                    next = response_chunk_length;
                    p->chunk_number = FALSE;
                }
                else {
                    p->chunk_number = TRUE;
                    p->response->body[p->body_count++] = c;
                    next = response_body;
                }
            } else
                next = response_headers;
        break;
    }

    return next;
}


static enum response_state
desiredHeader(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_desired_header;

    if(c == ':') {
        p->buffer[p->i] = 0;

        if(strcmp(p->buffer, "Content-Length") == 0)
            next = response_length;
        else if(strcmp(p->buffer, "Content-Type") == 0)
            next = response_media_type;
        else if(strcmp(p->buffer, "Transfer-Encoding") == 0)
            next = response_encoding;
        else
            next = response_headers;

        response_reset_buffer(p);
    } else
        p->buffer[p->i++] = c;


    return next;
}

static enum response_state
length(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_length;

    if(c == '\r') {
        p->response->length[p->i] = 0;
        response_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = response_enter;
    } else if(c != ' ')
        p->response->length[p->i++] = c;

    return next;
}

static enum response_state
mediaType(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_media_type;

    if(c == '\r') {
        p->response->mediaType[p->i] = 0;
        response_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = response_enter;
    } else if(c != ' ')
        p->response->mediaType[p->i++] = c;

    return next;
}

static enum response_state
encoding(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_encoding;

    if(c == '\r') {
        p->buffer[p->i] = 0;

        if(strstr(p->buffer, "chunked") != NULL)
            p->response->chunked = TRUE;

        response_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = response_enter;
    } else if(c != ' ')
        p->buffer[p->i++] = c;

    return next;
}


static enum response_state
chunk_length(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_chunk_length;


    if(c == '\r') {
        response_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = response_enter;
    }

    return next;
}

static enum response_state
body(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_body;

     if(c == '\r') {
         p->response->body[p->body_count] = 0;

         if(p->response->chunked) {
             response_reset_buffer(p);
             p->buffer[p->i++] = c;
             next = response_enter;
         } else
            next = response_done;
     } else
        p->response->body[p->body_count++] = c;

    return next;
}
