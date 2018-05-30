#include "responseParser.h"

typedef enum response_state {
    HEADERS,
    MEDIA_TYPE,
    LENGTH,
    ENCODING,
    BODY,
} response_state_t;


response_state_t checkContent(const char* inBuffer, int* i);
response_state_t checkEncoding(const char* inBuffer, int* i);

response_t* parseResponse(const char* inBuffer, int n) {

    char c;
    response_state_t state = HEADERS;
    response_t* response = malloc(sizeof(response_t));
    char length[BUFF_SIZE];
    response->mediaType = malloc(BUFF_SIZE);
    response->body = malloc(0);
    char encoding[BUFF_SIZE];
    int lengthCounter = 0, mediaTypeCounter = 0, bodyCounter = 0, bodyBuffer = 0, encodingCounter = 0;

    for(int i = 0; i < n; i++) {

        c = *(inBuffer + i);

        switch (state) {

            case HEADERS:
                if(c == 'C') {
                    state = checkContent(inBuffer, &i);
                } else if(c == 'T') {
                    state = checkEncoding(inBuffer, &i);
                } else if(c == '\n' && *(inBuffer + i + 1) == '\r' && *(inBuffer + i + 2) == '\n'){
                    state = BODY;
                    i += 2;
                }
            break;

            case LENGTH:
                if(c != '\r')
                    length[lengthCounter++] = c;
                else
                    state = HEADERS;
            break;

            case MEDIA_TYPE:
                if(c != '\r')
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

            case ENCODING:
            if(c != '\r')
                encoding[encodingCounter++] = c;
            else
                state = HEADERS;

            default:
                break;
        }
    }

    length[lengthCounter] = 0;
    encoding[encodingCounter] = 0;
    response->mediaType[mediaTypeCounter] = 0;
    response->body[bodyCounter] = 0;

    if(strstr(encoding, "chunked") != NULL) {
        size_t i;

        response->chunked = TRUE;

        for (i = 0; response->body[i] != '\r'; i++) {
            length[i] = response->body[i];
        }

        length[i] = 0;
        i++; //paso el \n
        response->body += i;
    }

    response->length = atoi(length);

    return response;
}

response_state_t checkContent(const char* inBuffer, int* i) {
    int size = sizeof("Content-Length:") - 1;
    response_state_t state = HEADERS;

    if( strncmp(inBuffer + *i, "Content-Length:", size) == 0) {

        state = LENGTH;
        *i += size;

    } else {

        size = sizeof("Content-Type:") - 1;

        if( strncmp(inBuffer + *i, "Content-Type:", size) == 0) {
            state = MEDIA_TYPE;
            *i += size;
        }
    }

    return state;
}

response_state_t checkEncoding(const char* inBuffer, int* i) {
    int size = sizeof("Transfer-Encoding:") - 1;
    response_state_t state = HEADERS;

    if( strncmp(inBuffer + *i, "Transfer-Encoding:", size) == 0) {
        state = ENCODING;
        *i += size;
    }

    return state;
}
