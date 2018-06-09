
#ifndef PC_2018_07_LOG_H
#define PC_2018_07_LOG_H

#include <stdio.h>
#include <sys/select.h>

struct request_data {
    struct sockaddr_storage origin_addr;
    struct sockaddr_storage client_addr;
    struct timeval start;
    struct timeval stop;
    char* request;
    int status_code;
};

struct request_data proxy_data[FD_SETSIZE];

void
init_file();

void
log_request(struct request_data data);


#endif //PC_2018_07_LOG_H
