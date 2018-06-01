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

#include "selector.h"

#define PORT 8090
#define LISTEN 30

static bool done = false;

static void sigterm_handler(const int signal){
    printf("Signal %d, graceful exit\n", signal);
    done = true;
}

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
        .handle_read = socksv5_passive_accept,
        .handle_write = NULL,
        .handle_close = NULL,
    };
    ss = selector_register(selector, mSocket, &socksv5, OP_READ, NULL);

    if(ss != SELECTOR_SUCCESS){
        DieWithUserMessage("ded", "registering master socket fd");
    }

    while(!done){
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS){
            DieWithUserMessage("ded", "serving");
        }
    }
    return 0;
}
