#include <arpa/inet.h>
#include <sys/time.h>
#include <stdlib.h>
#include "log.h"

struct request_data {
    struct sockaddr_storage origin_addr;
    struct sockaddr_storage client_addr;
    struct timeval start;
    struct timeval stop;
    char* request;
    int status_code;
};

static FILE* file;

static struct request_data proxy_data[FD_SETSIZE];

void
init_file() {
    file = fopen("access.log", "w");
    fprintf(file, "%-20s%-20s%-15s%-50s%-10s\n", "Client ip", "Server ip", "Duration (us)", "Request", "Status code");
}

void
close_file() {
    fclose(file);
}
static void
add_entry(struct sockaddr_storage origin_addr, struct sockaddr_storage client_addr,
          long int duration, char* request, int status_code) {

    char ip_client_buff[INET_ADDRSTRLEN];
    char ip_origin_buff[INET_ADDRSTRLEN];
    char str[10];

    sprintf(str, "%d", status_code);

    inet_ntop(client_addr.ss_family, &(((struct sockaddr_in *)&client_addr)->sin_addr),
            ip_client_buff, INET_ADDRSTRLEN);
    inet_ntop(origin_addr.ss_family, &(((struct sockaddr_in *)&origin_addr)->sin_addr),
            ip_origin_buff, INET_ADDRSTRLEN);

    fprintf(file, "%-20s%-20s%-15ld%-50s%-10s\n", ip_client_buff, ip_origin_buff,
            duration, request, status_code == 0 ? "-" : str);
}

void
log_request(int client_fd) {
    struct request_data data = proxy_data[client_fd];

    long int duration = data.start.tv_usec - data.stop.tv_usec;
    char request_header[1000];
    int i, status_code = data.status_code;


    if(data.request == NULL) {
        request_header[0] = '-';
        request_header[1] = 0;
    } else {
        for (i = 0; data.request[i] != '\r' && i<38; i++) {
            request_header[i] = data.request[i];
        }
        if(i == 38) {
            request_header[i++] = '.';
            request_header[i++] = '.';
            request_header[i++] = '.';
        }
        request_header[i] = 0;
    }

    add_entry(data.origin_addr, data.client_addr, duration, request_header, status_code);
}

void
register_request(int client_fd, char* request) {
    proxy_data[client_fd].request = request;
}

void
register_origin_addr(int client_fd, struct sockaddr_storage origin_addr) {
    proxy_data[client_fd].origin_addr = origin_addr;
}

void
register_client_addr(int client_fd, struct sockaddr_storage client_addr) {
    proxy_data[client_fd].client_addr = client_addr;
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


