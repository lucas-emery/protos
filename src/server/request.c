#include "request.h"
#include <errno.h>
#include <origin.h>
#include "utils.h"

static enum request_state
method(const uint8_t c, struct request_parser* p);

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

static enum request_state
content_length(const uint8_t c, struct request_parser* p);

static enum request_state
dest_port(const uint8_t c, struct request_parser* p);

void
request_parser_init (struct request_parser *p) {
    p->state = request_method;
    memset(p->request, 0, sizeof(*(p->request)));
    p->request->method = GET;                       //initialized so 405 doesn't trigger accidentally
    p->request->host = calloc(1, SMALLER_BUFF_SIZE);
    p->buffer = calloc(1, SMALLER_BUFF_SIZE);
    p->request->headers = calloc(1, BUFF_SIZE);
    p->i = 0;
}

extern request_state_t
request_consume(buffer *b, struct request_parser *p, bool *errored) {
    enum request_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       p->request->headers[p->request->headers_length++] = c;
       if(st == request_method) {
           st = request_parser_feed(p, c);
       } else {
           st = request_parser_feed(p, to_lower(c));
       }
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
    return st >= request_done;
}

void
request_close(struct request_parser *p) {
    free(p->buffer);
}

static void
request_reset_buffer(struct request_parser* p) {
    p->i = 0;
    bzero(p->buffer, SMALLER_BUFF_SIZE);
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
        case request_content_length:
            next = content_length(c, p);
            break;
        case request_enter:
            next = enter(c, p);
            break;
        case request_dest_port:
            next = dest_port(c, p);
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
        } else if(strcmp(p->buffer, "DELETE") == 0) {
            p->request->method = DELETE;
        } else if(strcmp(p->buffer, "PUT") == 0) {
            p->request->method = PUT;
        } else {
            p->request->method = UNSUPPORTED;
        }

        request_reset_buffer(p);
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
        p->buffer[p->i++] = c;
        next = request_enter;
    }

    return next;
}

static enum request_state
enter(const uint8_t c, struct request_parser* p) {

    switch(c){
        case '\n':
            p->buffer[p->i++] = c;
            if(strcmp(p->buffer, "\r\n\r\n") == 0)
                return request_done;
        break;
        case '\r':
            p->buffer[p->i++] = c;
        break;
        default:
            request_reset_buffer(p);
            if(c == 'h' || c == 'c') {
                p->buffer[p->i++] = c;
                return request_desired_header;
            }
            return request_headers;
    }

    return request_enter;
}

static enum request_state
desiredHeader(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_desired_header;

    if(c == ':') {
        p->buffer[p->i] = 0;

        if(strcmp(p->buffer, "host") == 0)
            next = request_host;
        else if(strcmp(p->buffer, "content-length") == 0) {
            next = request_content_length;
            request_reset_buffer(p);
        } else
            next = request_headers;

        request_reset_buffer(p);
    } else
        p->buffer[p->i++] = c;


    return next;
}


static enum request_state
content_length(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_content_length;

    if(c == '\r') {
        p->buffer[p->i] = 0;
        p->request->content_length = atoi(p->buffer);

        request_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = request_enter;
    } else if(c != ' ')
        p->buffer[p->i++] = c;

    return next;
}

static enum request_state
host(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_host;

    if(p->i > SMALLER_BUFF_SIZE)
        return request_error;

    if(c == '\r') {
        p->request->host[p->i] = 0;
        p->request->dest_port = 80;

        request_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = request_enter;
    } else if(c == ':') {
        p->request->host[p->i] = 0;
        request_reset_buffer(p);

        next = request_dest_port;

    } else {
        if(c != ' ')
            p->request->host[p->i++] = c;
    }


    return next;
}

static enum request_state
dest_port(const uint8_t c, struct request_parser* p) {
    enum request_state next = request_dest_port;

    if(c == '\r') {
        char *endptr;
        errno = 0;

        p->buffer[p->i] = 0;

        p->request->dest_port = (uint16_t) strtol(p->buffer, &endptr, 10);

        if (errno == ERANGE || *endptr != '\0' || p->buffer == endptr)
            return request_error;

        request_reset_buffer(p);
        p->buffer[p->i++] = c;
        next = request_enter;

    } else
        p->buffer[p->i++] = c;

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
