#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "selector.h"
#include "passive.h"
#include "request.h"
#include "netutils.h"
#include "sctpRequest.h"
#include "message.h"
#include "log.h"
#include "resolveLocalIp.h"

#include "transformation.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

#define PORT 8090
#define SCTP_PORT 9095
#define LISTEN 30
#define SCTP_LISTEN 5

//#define DEBUG

static bool done = false;

enum type{
    NONE,
    CLIENT,
    ORIGIN
};

typedef struct {
    enum type type;
    int peer;
    char host[64];
    char state[32];
} table_entry_t;

table_entry_t table[1024];


void * print_table(){
    while(1){
        system("clear");
        for(int i = 0; i < 128 ;i++){
            if(table[i].type == CLIENT){
                int peer = -1;
                for(int j = 0; j < 128; j++){
                    if(table[j].type == ORIGIN && table[j].peer == i){
                        peer = j;
                        break;
                    }
                }
                if(peer != -1)
                    printf("CLIENT:\t%d\t%-32s\t%-20s\tORIGIN:\t%d\t%s\n", i, table[i].host, table[i].state, peer, table[peer].state);
                else
                    printf("CLIENT:\t%d\t%-32s\t%-20s\n", i, table[i].host, table[i].state);

            }
        }
        usleep(1000*50);//50ms
    }
}

static void sigterm_handler(const int signal){
    close_log_file();
    free_ips();
    close_transformations();
    done = true;
}

//void
//log_request(const enum socks_response_status status,
//            const struct sockaddr* clientaddr,
//            const struct sockaddr* originaddr) {
//    char cbuff[SOCKADDR_TO_HUMAN_MIN * 2 + 2 + 32] = { 0 };
//    unsigned n = N(cbuff);
//    time_t now = 0;
//    time(&now);
//
//    // tendriamos que usar gmtime_r pero no está disponible en C99
//    strftime(cbuff, n, "%FT%TZ\t", gmtime(&now));
//    size_t len = strlen(cbuff);
//    sockaddr_to_human(cbuff + len, N(cbuff) - len, clientaddr);
//    strncat(cbuff, "\t", n-1);
//    cbuff[n-1] = 0;
//    len = strlen(cbuff);
//    sockaddr_to_human(cbuff + len, N(cbuff) - len, originaddr);
//
//    fprintf(stdout, "%s\tstatus=%d\n", cbuff, status);
//}

int get_params(const int argc, const char ** argv, char * ip_tcp, char * ip_sctp, int * port_tcp, int * port_sctp) {
    int i;
    char type;
    for(i=1; i<argc; i++) {
        if(argv[i][0] == '-') {
            type = argv[i++][1];
            if(i < argc) {
                switch(type) {
                    case 'p':
                        if(sscanf(argv[i], "%d", port_tcp) != 1) {
                            return 0;
                        }
                        break;
                    case 'P':
                        if(sscanf(argv[i], "%d", port_sctp) != 1) {
                            return 0;
                        }
                        break;
                    case 'l':
                        if(sscanf(argv[i], "%s", ip_tcp) != 1) {
                            return 0;
                        }
                        break;
                    case 'L':
                        if(sscanf(argv[i], "%s", ip_sctp) != 1) {
                            return 0;
                        }
                        break;
                    default:
                        return 0;
                }
            } else {
                return 0;
            }

        } else {
            return 0;
        }
    }

    return 1;
}

int main(const int argc, const char **argv){
    char ip_tcp[100] = {0}, ip_sctp[100] = {0};
    int port_tcp = PORT, port_sctp = SCTP_PORT;

    if(!get_params(argc, argv, ip_tcp, ip_sctp, &port_tcp, &port_sctp)) {
        printf("Invalid argument\n");
        return 1;
    }

    local_ip_resolv(port_tcp);

    close(0);

    selector_status ss = SELECTOR_SUCCESS;
    fd_selector selector = NULL;
    int mSocket;
    struct sockaddr_in serv_addr4;
    struct sockaddr_in6 serv_addr6;
    bool is_ipv4 = false;

    char buff[INET_ADDRSTRLEN];
    if(inet_pton(AF_INET, ip_tcp, buff) > 0) { //is ipv4
        is_ipv4 = true;
        memset(&serv_addr4, 0, sizeof(serv_addr4)); // Zero out structure
        serv_addr4.sin_family = AF_INET;
        serv_addr4.sin_port = htons(PORT);   // Local port

        if(ip_tcp[0] == 0)
            serv_addr4.sin_addr.s_addr = htonl(INADDR_ANY);
        else {
            if(inet_pton(AF_INET, ip_tcp, &serv_addr4.sin_addr) < 0)
                DieWithUserMessage("ded", "Incorrect ip");
        }

        mSocket =  socket(AF_INET, SOCK_STREAM, 0);

    } else {
        memset(&serv_addr6, 0, sizeof(serv_addr6)); // Zero out structure
        serv_addr6.sin6_family = AF_INET6;        // IPv6 address family
        serv_addr6.sin6_port = htons(PORT);   // Local port

        if(ip_tcp[0] == 0)
            serv_addr6.sin6_addr = in6addr_any;
        else {
            if (inet_pton(AF_INET6, ip_tcp, &serv_addr6.sin6_addr) < 0)
                DieWithUserMessage("ded", "Incorrect ip");
        }

        mSocket =  socket(AF_INET6, SOCK_STREAM, 0);
    }

    if(mSocket < 0){
        DieWithUserMessage("ded", "creating master socket");
    }

    #ifdef DEBUG
        pthread_t t;
        pthread_create(&t, NULL, print_table, NULL);
    #endif

    printf("Listening on TCP port %d\n", PORT);

    setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(is_ipv4) {
        if (bind(mSocket, (struct sockaddr *) &serv_addr4, sizeof(serv_addr4)) < 0) {
            DieWithUserMessage("ded", "binding master socket");
        }
    } else {
        if (bind(mSocket, (struct sockaddr *) &serv_addr6, sizeof(serv_addr6)) < 0) {
            DieWithUserMessage("ded", "binding master socket");
        }
    }

    if(listen(mSocket, LISTEN) < 0){
        DieWithUserMessage("ded", "master socket listening");
    }

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    if(selector_fd_set_nio(mSocket) == -1){
        DieWithUserMessage("ded", "getting master socket flags");
    }
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec = 10,
            .tv_nsec = 0,
        }
    };

    if(selector_init(&conf) != 0){
        DieWithUserMessage("ded", "initializing selector");
    }

    selector = selector_new(1024);
    if(selector == NULL) {
        DieWithUserMessage("ded", "creating selector");
    }
    const struct fd_handler socksv5 = {
        .handle_read = socks_passive_accept,
        .handle_write = NULL,
        .handle_close = NULL,
    };
    ss = selector_register(selector, mSocket, &socksv5, OP_READ, NULL);

    if(ss != SELECTOR_SUCCESS){
        DieWithUserMessage("ded", "registering master socket fd");
    }

    const int sctp_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    if(sctp_socket < 0)
        DieWithUserMessage("ded", "creating sctp socket");

    struct sockaddr_in sctp_addr;
    memset(&sctp_addr, 0, sizeof(sctp_addr)); 
    sctp_addr.sin_family = AF_INET;
    sctp_addr.sin_port = htons(port_sctp);
    if(strlen(ip_sctp) == 0) {
        sctp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_ntop(AF_INET, &sctp_addr.sin_addr.s_addr, ip_sctp, strlen(ip_sctp));
    }

    if(bind(sctp_socket, (struct sockaddr *)&sctp_addr, sizeof(sctp_addr)) < 0) {
        DieWithUserMessage("ded", "binding sctp socket");
    }

    struct sctp_initmsg initmsg;
    memset(&initmsg, 0, sizeof (initmsg));
    initmsg.sinit_num_ostreams = 5;
    initmsg.sinit_max_instreams = 5;
    initmsg.sinit_max_attempts = 4;
    
    if(setsockopt (sctp_socket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof (initmsg)) < 0) {
        DieWithUserMessage("ded", "setsockopt sctp failed");
    }

    if(listen(sctp_socket, SCTP_LISTEN) < 0){
        DieWithUserMessage("ded", "sctp socket listening");
    }

    if(selector_fd_set_nio(sctp_socket) == -1){
        DieWithUserMessage("ded", "getting sctp socket flags");
    }

    const struct fd_handler sctp_socksv5 = {
        .handle_read = sctp_socks_accept,
        .handle_write = NULL,
        .handle_close = NULL,
    };

    ss = selector_register(selector, sctp_socket, &sctp_socksv5, OP_READ, NULL);

    if(ss != SELECTOR_SUCCESS){
        DieWithUserMessage("ded", "registering sctp socket fd");
    }

    init_log();

    while(!done){
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS){
            DieWithUserMessage("ded", "serving");
        }
    }

    if(selector != NULL) {
        selector_destroy(selector);
    }

    selector_close();
    return 0;
}
