#include <arpa/inet.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "log.h"

#define REQUEST_LENGTH 43

struct request_data {
    struct sockaddr_storage* origin_addr;
    struct sockaddr_storage* client_addr;
    struct timeval start;
    struct timeval stop;
    char* request;
    int status_code;
};

static FILE* file;
static struct request_data* proxy_data;

void
init_log() {
    file = fopen("access.log", "w");
    fprintf(file, "%-20s%-20s%-15s%-50s%-10s\n", "Client ip", "Server ip", "Duration (\xC2\xB5s)", "Request", "Status code");

    proxy_data = calloc(1, FD_SETSIZE * sizeof(struct request_data));

    for (int i = 0; i < FD_SETSIZE; ++i) {
        proxy_data[i].origin_addr = calloc(1, sizeof(struct sockaddr_storage) );
        proxy_data[i].client_addr = calloc(1, sizeof(struct sockaddr_storage) );
        proxy_data[i].request = calloc(1, 100);
    }
}

void
close_log_file() {
    fclose(file);

    for (int i = 0; i < FD_SETSIZE; ++i) {
        free(proxy_data[i].origin_addr);
        free(proxy_data[i].client_addr);
        free(proxy_data[i].request);
    }

    free(proxy_data);
}

static void
add_entry(struct sockaddr_storage* origin_addr, struct sockaddr_storage* client_addr,
          long int duration, char* request, int status_code) {

    char ip_client_buff[INET6_ADDRSTRLEN] = {0};
    char ip_origin_buff[INET6_ADDRSTRLEN] = {0};
    char str1[10] = {0};

    sprintf(str1, "%d", status_code);

    if(inet_ntop(client_addr->ss_family, &(((struct sockaddr_in *)client_addr)->sin_addr),
            ip_client_buff, INET6_ADDRSTRLEN) <= 0);
    if(inet_ntop(origin_addr->ss_family, &(((struct sockaddr_in *)origin_addr)->sin_addr),
                 ip_origin_buff, INET6_ADDRSTRLEN) <= 0);

    fprintf(file, "%-20s%-20s%-15lu%-50s%-10s\n", ip_client_buff, ip_origin_buff,
            duration, request, str1);
}

void
log_request(int client_fd) {
    struct request_data data = proxy_data[client_fd];

    if(data.request != NULL && data.origin_addr != NULL && data.client_addr != NULL && data.origin_addr != 0) {
        long int duration = llabs(data.start.tv_usec - data.stop.tv_usec);

        add_entry(data.origin_addr, data.client_addr, duration, data.request, data.status_code);
    }
}

void
register_request(int client_fd, char* request) {
    int i;

    for (i = 0; request[i] != '\r' && i <= REQUEST_LENGTH; i++);

    strncpy(proxy_data[client_fd].request, request, i);

    if(i >= REQUEST_LENGTH) {
        proxy_data[client_fd].request[i++] = '.';
        proxy_data[client_fd].request[i++] = '.';
        proxy_data[client_fd].request[i++] = '.';
    }
}

void
register_origin_addr(int client_fd, struct sockaddr_storage* origin_addr) {
    memcpy(proxy_data[client_fd].origin_addr, origin_addr, sizeof(struct sockaddr_storage));
}

void
register_client_addr(int client_fd, struct sockaddr_storage* client_addr) {
    memcpy(proxy_data[client_fd].client_addr, client_addr, sizeof(struct sockaddr_storage));
}

void
register_status_code(int client_fd, int status_code) {
    proxy_data[client_fd].status_code = status_code;
}

void
register_start(int client_fd) {
    gettimeofday(&proxy_data[client_fd].start, NULL);
}

void
register_stop(int client_fd) {
    gettimeofday(&proxy_data[client_fd].stop, NULL);
}


