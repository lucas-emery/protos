#ifndef PC_2018_07_ORIGIN_H
#define PC_2018_07_ORIGIN_H

#include <stdint.h>
#include <time.h>
#include "stm.h"
#include "buffer.h"
#include "response.h"

#define ORIGIN_ATTACHMENT(key) ( (origin_t *)(key)->data)

typedef struct {
    int origin_fd;
    int client_fd;
    int infd, outfd;
    buffer buff;
    struct response response;
    struct response_parser parser;
    bool readFirst;

    buffer *rb , *wb, *tb;
    bool * respDone, *reqDone, *transDone;
    uint8_t  raw_data[BUFF_SIZE];

    struct timeval time;

    struct state_machine stm;
} origin_t;

#endif //PC_2018_07_ORIGIN_H
