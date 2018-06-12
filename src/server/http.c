#include <stdbool.h>
#include <memory.h>
#include <passive.h>
#include <origin.h>
#include "http.h"

static const char* HTTP_405 = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
static const char* HTTP_409 = "HTTP/1.1 409 Conflict\r\nContent-Length: 0\r\n\r\n";
static const char* HTTP_502 = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
static const char* HTTP_504 = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n";
static const char* HTTP_505 = "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 0\r\n\r\n";
static const char* HTTP_508 = "HTTP/1.1 508 Loop Detected\r\nContent-Length: 0\r\n\r\n";

bool send_http_code_fd(unsigned code, fd_selector s, int fd, buffer * wb, bool * reqDone, bool * respDone, bool * transDone);


bool send_http_code_from_client(unsigned code, struct selector_key * key) {
    client_t * c = CLIENT_ATTACHMENT(key);
    return send_http_code_fd(code, key->s, key->fd, &c->write_buffer, c->reqDone, c->respDone, c->transDone);
}

bool send_http_code_from_origin(unsigned code, struct selector_key * key) {
    origin_t * o = ORIGIN_ATTACHMENT(key);
    return send_http_code_fd(code, key->s, o->client_fd, o->rb, o->reqDone, o->respDone, o->transDone);
}

bool send_http_code_fd(unsigned code, fd_selector s, int fd, buffer * b, bool * reqDone, bool * respDone, bool * transDone) {
    size_t size, length;
    const char * message;
    bool lastResponse = false;
    uint8_t * ptr = buffer_write_ptr(b, &size);


    switch (code) {
        case 405:
            message = HTTP_405;
            lastResponse = true;
            break;
        case 409:
            message = HTTP_409;
            lastResponse = true;
            break;
        case 502:
            message = HTTP_502;
            lastResponse = true;
            break;
        case 504:
            message = HTTP_504;
            lastResponse = true;
            break;
        case 505:
            message = HTTP_505;
            lastResponse = true;
            break;
        case 508:
            message = HTTP_508;
            lastResponse = true;
            break;
        default:
            message = NULL;
            break;
    }

    if(message != NULL) {
        length = strlen(message);
        if(size >= length) {
            memcpy(ptr, message, length);
            buffer_write_adv(b, length);
            if(lastResponse) {
                selector_set_interest(s, fd, OP_WRITE);
                *reqDone = true;
                *respDone = true;
                *transDone = true;
            } else {
                selector_add_interest(s, fd, OP_WRITE);
            }
            return true;
        }
    }
    return false;
}