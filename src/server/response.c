#include "response.h"
#include "utils.h"

static enum response_state
version(const uint8_t c, struct response_parser* p);

static enum response_state
status_code(const uint8_t c, struct response_parser* p);

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


static void
response_reset_buffer(struct response_parser* p);

bool
body_is_done(struct response_parser *p, int length) {
    increase_body_length(p, -length);
    return p->response->body_length == 0 ? true : false;
}

void
increase_body_length(struct response_parser *p, int length) {
    if(!p->response->chunked)
        p->response->body_length += length;
}

bool
chunked_is_done(uint8_t * buffer, int length) {
  uint8_t * end = buffer + length - 4;
  return *end == '\r' && *(end+1) == '\n' && *(end + 2) == '\r' && *(end + 3) == '\n';
}

extern enum response_state
response_consume(buffer *b, struct response_parser *p, bool *errored) {
    enum response_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       p->response->headers[p->response->header_length++] = c;
       st = response_parser_feed(p, to_lower(c));
       if(response_is_done(st, errored)) {
          break;
       }
    }

    return st;
}

void
parser_headers(struct response_parser *p, uint8_t * ptr) {
    p->response->headers = ptr;
}

void
response_parser_init (struct response_parser *p) {
    p->state = response_version;
    memset(p->response, 0, sizeof(*(p->response)));
    p->response->mediaType = malloc(BUFF_SIZE);
    p->response->chunked = FALSE;
    p->response->headers = malloc(BUFF_SIZE);
    p->response->header_length = 0;
    p->response->body_length = 0;
    p->buffer = malloc(BUFF_SIZE);
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
        case response_version:
            next = version(c, p);
            break;
        case response_status_code:
            next = status_code(c, p);
            break;
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
        case response_enter:
            next = enter(c, p);
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
            if(strcmp(p->buffer, "\r\n\r\n") == 0)
                next = response_done;
        break;
        case '\r':
            p->buffer[p->i++] = c;
        break;
        default:
            if(c == 't' || c == 'c') {
                response_reset_buffer(p);
                p->buffer[p->i++] = c;
                next = response_desired_header;
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

        if(strcmp(p->buffer, "content-length") == 0)
            next = response_length;
        else if(strcmp(p->buffer, "content-type") == 0)
            next = response_media_type;
        else if(strcmp(p->buffer, "transfer-encoding") == 0)
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
        p->buffer[p->i] = 0;

        p->response->body_length = atoi(p->buffer);

        response_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = response_enter;
    } else if(c != ' ')
        p->buffer[p->i++] = c;

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
version(const uint8_t c, struct response_parser* p) {

    if(c == ' ')
        return response_status_code;

    return response_version;
}

static enum response_state
status_code(const uint8_t c, struct response_parser* p) {
    enum response_state next = response_status_code;

    if(c == ' ') {
        p->buffer[p->i] = 0;

        p->response->status_code = atoi(p->buffer);

        response_reset_buffer(p);
        next = response_headers;
    } else {
        p->buffer[p->i++] = c;
    }

    return next;
}
