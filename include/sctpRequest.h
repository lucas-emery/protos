#ifndef SCTPREQUEST_H_
#define SCTPREQUEST_H_

#include "stm.h"
#include "netutils.h"
#include "selector.h"
#include "transformation.h"
#include "metrics.h"

#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>
#include <arpa/inet.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#define METRIC_SIZE 64
#define METRIC '0'
#define CONFIGURATION '1'
#define METRIC_ERROR 2
#define CONFIGURATION_ERROR 3
#define MAX_BUFFER_SIZE 256
#define MEDIATYPE_SIZE 20


typedef struct {
    uint8_t * read_buffer, * write_buffer;
} sctp_request_st;

typedef struct {
    /** información del cliente */
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        sctp_request_st         request;
    } client;

    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t * read_buffer, * write_buffer;

} sctp_client_t;

typedef enum {
	SCTP_READ,
	SCTP_WRITE,
	SCTP_DONE,
	SCTP_ERROR
} sctp_sock_state_t;

int sctp_request_parser(uint8_t * read_buffer, uint8_t * write_buffer, int n);
void getMetric(char type, char * metric);
int applyFilter(char type, char * mediaType);
void sctp_socks_accept(struct selector_key *key);

#endif