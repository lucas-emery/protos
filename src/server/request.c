#include "request.h"
#include <errno.h>

static enum request_state
method(const uint8_t c, struct request_parser* p);

static enum request_state
body(const uint8_t c, struct request_parser* p);

static enum request_state
desiredHeader(const uint8_t c, struct request_parser* p);

static enum request_state
headers(const uint8_t c, struct request_parser* p);

static enum request_state
host(const uint8_t c, struct request_parser* p);

static enum request_state
enter(const uint8_t c, struct request_parser* p);

static void
request_reset_buffer(struct request_parser* p) ;

void
request_log() {

}

void
request_parser_init (struct request_parser *p) {
    p->state = request_method;
    p->request = malloc(sizeof( *(p->request) ));
    p->request->host = malloc(BUFF_SIZE);
    p->buffer = malloc(BUFF_SIZE);
}

extern request_state_t
request_consume(buffer *b, struct request_parser *p, bool *errored) {
    enum request_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       st = request_parser_feed(p, c);
       if(request_is_done(p, st, errored)) {
          break;
       }
    }

    return st;
}

bool
request_is_done(struct request_parser *p, const enum request_state st, bool *errored) {
    if(st >= request_error && errored != 0) {
        *errored = true;
    }
    return st >= request_done || (st == request_body && (p->request->method == GET || p->request->method == HEAD));
}

void
request_close(struct request_parser *p) {
    free(p->buffer);
}

static void
request_reset_buffer(struct request_parser* p) {
    p->i = 0;
    bzero(p->buffer, BUFF_SIZE);
}

request_state_t
request_parser_feed (struct request_parser *p, const uint8_t c) {
    enum request_state next;

    switch(p->state) {
        case request_method:
            next = method(c, p);
            break;
        case request_headers:
            next = headers(c, p);
            break;
        case request_desired_header:
            next = desiredHeader(c, p);
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

    if(c == '\r') {
        request_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = request_enter;
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
            request_reset_buffer(p);
            if(c == 'H') {
                p->buffer[p->i++] = c;
                next = request_desired_header;
            } else
                next = request_headers;
        break;
    }

    return next;
}

static enum request_state
desiredHeader(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_desired_header;

    if(c == ':') {
        p->buffer[p->i] = 0;

        if(strcmp(p->buffer, "Host") == 0)
            next = request_host;
        else
            next = request_headers;

        request_reset_buffer(p);
    } else
        p->buffer[p->i++] = c;


    return next;
}


static enum request_state
host(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_host;

    if(c == '\r') {
        p->request->host[p->i] = 0;
        next = request_headers;
    } else if(c != ' ')
        p->request->host[p->i++] = c;

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

enum socks_response_status
errno_to_socks(const int e) {
    enum socks_response_status ret = status_general_SOCKS_server_failure;
    switch (e) {
        case 0:
            ret = status_succeeded;
            break;
        case ECONNREFUSED:
            ret = status_connection_refused;
            break;
        case EHOSTUNREACH:
            ret = status_host_unreachable;
            break;
        case ENETUNREACH:
            ret = status_network_unreachable;
            break;
        case ETIMEDOUT:
            ret = status_ttl_expired;
            break;
    }
    return ret;
}
