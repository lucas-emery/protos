
#ifndef PC_2018_07_LOG_H
#define PC_2018_07_LOG_H

#include <stdio.h>
#include <sys/select.h>


void
log_request(int client_fd);

void
init_log();

void
register_request(int client_fd, char* request);

void
register_origin_addr(int client_fd, struct sockaddr_storage* origin_addr);

void
register_client_addr(int client_fd, struct sockaddr_storage* client_addr);

void
register_status_code(int client_fd, int status_code);

void
register_start(int client_fd);

void
register_stop(int client_fd);


#endif //PC_2018_07_LOG_H
