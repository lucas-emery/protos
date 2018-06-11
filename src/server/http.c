#include <stdbool.h>
#include <memory.h>
#include <passive.h>
#include "http.h"

static const char* HTTP_405 = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
static const char* HTTP_409 = "HTTP/1.1 409 Conflict\r\nContent-Length: 0\r\n\r\n";

bool send_http_code(unsigned code, struct selector_key * key) {

    client_t * c = CLIENT_ATTACHMENT(key);
    buffer * b = &c->write_buffer;

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
                selector_set_interest_key(key, OP_WRITE);
                *c->reqDone = true;
                *c->respDone = true;
            } else {
                selector_add_interest_key(key, OP_WRITE);
            }
            return true;
        }
    }
    return false;
}