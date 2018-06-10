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

#include "selector.h"
#include "passive.h"
#include "request.h"
#include "netutils.h"
#include "sctpRequest.h"
#include "message.h"
#include "log.h"


#define N(x) (sizeof(x)/sizeof((x)[0]))

#define PORT 8090
#define SCTP_PORT 9090
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

table_entry_t table[128];

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
    printf("Signal %d, graceful exit\n", signal);
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

int main(const int argc, const char **argv){
    close(0);

    selector_status ss = SELECTOR_SUCCESS;
    fd_selector selector = NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    const int mSocket =  socket(AF_INET, SOCK_STREAM, 0);
    if(mSocket < 0){
        DieWithUserMessage("ded", "creating master socket");
    }

    #ifdef DEBUG
        pthread_t t;
        pthread_create(&t, NULL, print_table, NULL);
    #endif

    printf("Listening on TCP port %d\n", PORT);

    setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(mSocket, (struct sockaddr*) &addr, sizeof(addr)) <  0){
        DieWithUserMessage("ded", "binding master socket");
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
    sctp_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    sctp_addr.sin_port = htons (SCTP_PORT);

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
