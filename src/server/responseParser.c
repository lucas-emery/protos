#include "responseParser.h"

typedef enum response_state {
    HEADERS,
    MEDIA_TYPE,
    LENGTH,
    ENCODING,
    BODY,
} response_state_t;

response_t* parseResponse(const char* inBuffer, int n) {

    char c;
    response_state_t state = HEADERS;
    response_t* response = malloc(sizeof(response_t));
    char length[BUFF_SIZE];
    response->mediaType = malloc(BUFF_SIZE);
    response->body = malloc(0);
    int lengthCounter = 0;
    int mediaTypeCounter = 0;
    int bodyCounter = 0;
    int bodyBuffer = 0;

    for(int i = 0; i < n; i++) {

        c = *(inBuffer + i);

        switch (state) {

            case HEADERS:
                if(c == 'C') {
                    int size = sizeof("Content-Length:") - 1;
                    if( strncmp(inBuffer + i, "Content-Length:", size) == 0) {
                        state = LENGTH;
                        i += size;
                    } else {
                        size = sizeof("Content-Type:") - 1;
                        if( strncmp(inBuffer + i, "Content-Type:", size) == 0) {
                            state = MEDIA_TYPE;
                            i += size;
                        }
                    }
                } else if(c == 'T') {
                    int size = sizeof("Transfer-Encoding:") - 1;
                    if( strncmp(inBuffer + i, "Transfer-Encoding:", size) == 0) {
                        state = ENCODING;
                        i += size;
                    }
                } else if(c == '\n' && *(inBuffer + i + 1) == '\r' && *(inBuffer + i + 2) == '\n'){
                    state = BODY;
                    i += 2;
                }
            break;

            case LENGTH:
                if(c != '\n')
                    length[lengthCounter++] = c;
                else
                    state = HEADERS;
            break;

            case MEDIA_TYPE:
                if(c != '\n')
                    response->mediaType[mediaTypeCounter++] = c;
                else
                    state = HEADERS;
            break;

            case BODY:
                if(c != '\r') {
                    if(bodyCounter % BUFF_SIZE == 0) {
                        bodyBuffer++;
                        response->body = realloc(response->body, bodyBuffer * BUFF_SIZE);
                    }
                    response->body[bodyCounter++] = c;
                }
            break;

            default:
                break;
        }
    }

    length[lengthCounter] = 0;
    response->mediaType[mediaTypeCounter] = 0;
    response->body[bodyCounter] = 0;
    response->length = atoi(length);

    return response;
}
