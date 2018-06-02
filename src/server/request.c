#include "request.h"

static enum request_state
method(const uint8_t c, struct request_parser* p);

static enum request_state
body(const uint8_t c, struct request_parser* p);

static enum request_state
finishBody(const uint8_t c, struct request_parser* p);

static enum request_state
headers(const uint8_t c, struct request_parser* p);

static enum request_state
hostHeader(const uint8_t c, struct request_parser* p);

static enum request_state
host(const uint8_t c, struct request_parser* p);

static enum request_state
finishHeaders(const uint8_t c, struct request_parser* p);

static enum request_state
enter(const uint8_t c, struct request_parser* p);

void
request_log() {

}

static void
remaining_set(struct request_parser* p, int n) {
    p->i = 0;
    p->remaining = n;
}

static int
remaining_is_done(struct request_parser* p) {
    return p->i >= p->remaining;
}

void
request_parser_init (struct request_parser *p) {
    p->state = request_method;
    p->request = malloc(sizeof( *(p->request) ));
    p->request->host = malloc(BUFF_SIZE);
    p->buffer = malloc(BUFF_SIZE);
}

extern enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored) {
    enum request_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       st = request_parser_feed(p, c);
       if(request_is_done(st, errored)) {
          break;
       }
    }

    return st;
}

bool
request_is_done(const enum request_state st, bool *errored) {
    if(st >= request_error && errored != 0) {
        *errored = true;
    }
    return st >= request_done;
}

void
request_close(struct request_parser *p) {
    free(p->buffer);
}

enum request_state
request_parser_feed (struct request_parser *p, const uint8_t c) {
    enum request_state next;

    switch(p->state) {
        case request_method:
            next = method(c, p);
            break;
        case request_headers:
            next = headers(c, p);
            break;
        case request_host:
            next = host(c, p);
            break;
        case request_body:
            next = body(c, p);
            break;
        case request_enter:
            next = enter(c, p);
            break;
        case request_done:
        case request_error:
        default:
            next = request_error;
            break;
    }

    return p->state = next;
}

static enum request_state
method(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_method;

    if(c == ' ') {
        p->buffer[p->i] = 0;

        if(strcmp(p->buffer, "GET") == 0) {
            p->request->method = GET;
        } else if(strcmp(p->buffer, "POST") == 0) {
            p->request->method = POST;
        } else if(strcmp(p->buffer, "HEAD") == 0) {
            p->request->method = HEAD;
        } else if(strcmp(p->buffer, "CONNECT") == 0) {
            p->request->method = CONNECT;
        } else {
            next = request_error;
        }

        bzero(p->buffer, BUFF_SIZE);
        next = request_headers;
    } else {
        p->buffer[p->i++] = c;
    }

    return next;
}

static enum request_state
headers(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_headers;

    switch (c) {
        case 'H':
            bzero(p->buffer, BUFF_SIZE);
            p->i = 0;
            p->buffer[p->i++] = c;
        break;
        case 'o':
            if(strcmp(p->buffer, "H") == 0)
                p->buffer[p->i++] = c;
        break;
        case 's':
            if(strcmp(p->buffer, "Ho") == 0)
                p->buffer[p->i++] = c;
        break;
        case 't':
            if(strcmp(p->buffer, "Hos") == 0)
                p->buffer[p->i++] = c;
        break;
        case ':':
            if(strcmp(p->buffer, "Host") == 0){
                next = request_host;
            }
            p->i = 0;
            bzero(p->buffer, BUFF_SIZE);
        break;
        case '\r':
        p->i = 0;
        bzero(p->buffer, BUFF_SIZE);
            p->buffer[p->i++] = c;
            next = request_enter;
        break;
    }

    return next;
}

static enum request_state
enter(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_enter;

    switch(c){
        case '\n':
            p->buffer[p->i++] = c;
            if(strcmp(p->buffer, "\r\n\r\n") == 0) {
                next = request_body;
            }
        break;
        case '\r':
            p->buffer[p->i++] = c;
        break;
        default:
            bzero(p->buffer, BUFF_SIZE);
            p->i = 0;
            p->buffer[p->i++] = c;
            next = request_headers;
        break;
    }

    return next;
}


static enum request_state
host(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_host;

    if(c == '\r') {
        p->request->host[p->i] = 0;
        next = request_headers;
    } else {
        if(c != ' ')
            p->request->host[p->i++] = c;
    }

    return next;
}

static enum request_state
body(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_body;

     if(c == '\r')
        next = request_done;

    //TODO chunked

    return next;
}
