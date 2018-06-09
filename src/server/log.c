#include <arpa/inet.h>
#include <sys/time.h>
#include "log.h"

static FILE* file;

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

    inet_ntop(client_addr.ss_family, &(((struct sockaddr_in *)&client_addr)->sin_addr), ip_client_buff, INET_ADDRSTRLEN);
    inet_ntop(origin_addr.ss_family, &(((struct sockaddr_in *)&origin_addr)->sin_addr), ip_origin_buff, INET_ADDRSTRLEN);

    fprintf(file, "%-20s%-20s%-15ld%-50s%-10d\n", ip_client_buff, ip_origin_buff, duration, request, status_code);
}

void
log_request(struct request_data data) {
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


