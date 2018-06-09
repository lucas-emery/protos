#include "sctpRequest.h"

#define CLIENT_ATTACHMENT(key) ( (sctp_client_t *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))

static char password[] = "holapete";

static void sctp_request_init(const unsigned state, struct selector_key *key) {
    sctp_request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

    d->read_buffer = CLIENT_ATTACHMENT(key)->read_buffer;
    d->write_buffer = CLIENT_ATTACHMENT(key)->write_buffer;
}

static unsigned sctp_request_read(struct selector_key *key) {
    sctp_request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

    struct sctp_sndrcvinfo sndrcvinfo;
    int flags, n;

    bzero(d->read_buffer, MAX_BUFFER_SIZE);
    n = sctp_recvmsg(key->fd, (void *) d->read_buffer, (size_t) MAX_BUFFER_SIZE, (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
    if(n > 0) {
    	if(sctp_request_parser(d->read_buffer, d->write_buffer, n) > 0) {
        	selector_set_interest_key(key, OP_WRITE);
        	return SCTP_WRITE;
        }
        else
            return SCTP_ERROR;
    }
    else
    {
        printf("%d\n", n);
    	perror("sctp_recvmsg()");
    	return SCTP_ERROR;
    }
}

static void sctp_request_read_close(const unsigned state, struct selector_key *key) {

}

static unsigned sctp_request_write(struct selector_key *key) {
    sctp_request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

    int n = sctp_sendmsg(key->fd, (void *) d->write_buffer, (size_t) MAX_BUFFER_SIZE, (struct sockaddr *) NULL, 0, 0, 0, 0, 0, 0);
    if(n == -1) {
        return SCTP_ERROR;
    }

    selector_set_interest_key(key, OP_READ);
    return SCTP_DONE;
}

static const struct state_definition sctp_client_statbl[] = {
    {
        .state            = SCTP_READ,
        .on_arrival       = sctp_request_init,
        .on_departure     = sctp_request_read_close,
        .on_read_ready    = sctp_request_read,
    },{
        .state            = SCTP_WRITE,
        .on_write_ready   = sctp_request_write,
    },{
        .state            = SCTP_DONE,

    },{
        .state            = SCTP_ERROR,
    }
};

static const struct state_definition * sctp_client_describe_states(void) {
    return sctp_client_statbl;
}

static sctp_client_t * sctp_client_new(int client_fd) {
    sctp_client_t *ret;

    ret = malloc(sizeof(*ret));

    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->client_fd       = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);

    ret->stm    .initial   = SCTP_READ;
    ret->stm    .max_state = SCTP_ERROR;
    ret->stm    .states    = sctp_client_describe_states();
    stm_init(&ret->stm);

    ret->read_buffer = malloc(MAX_BUFFER_SIZE);
    ret->write_buffer = malloc(MAX_BUFFER_SIZE);

finally:
    return ret;
}


static void sctp_client_done(struct selector_key* key) {
    const int fd = CLIENT_ATTACHMENT(key)->client_fd;
    if(fd != -1) {
        if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fd)) {
            abort();
        }
        close(fd);
    }
}

static void sctp_client_read(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sctp_sock_state_t st = (const sctp_sock_state_t) stm_handler_read(stm, key);

    if(SCTP_ERROR == st || SCTP_DONE == st) {
        sctp_client_done(key);
    }
}

static void sctp_client_write(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sctp_sock_state_t st = (const sctp_sock_state_t) stm_handler_write(stm, key);

    if(SCTP_ERROR == st || SCTP_DONE == st) {
        sctp_client_done(key);
    }
}

static void sctp_client_block(struct selector_key *key) {
    struct state_machine *stm   = &CLIENT_ATTACHMENT(key)->stm;
    const sctp_sock_state_t st = (const sctp_sock_state_t) stm_handler_block(stm, key);

    if(SCTP_ERROR == st || SCTP_DONE == st) {
        sctp_client_done(key);
    }
}

static void sctp_client_destroy(sctp_client_t* s) {
    free(s);
}

static void sctp_client_close(struct selector_key *key) {
    sctp_client_destroy(CLIENT_ATTACHMENT(key));
}

static const struct fd_handler client_handler = {
    .handle_read   = sctp_client_read,
    .handle_write  = sctp_client_write,
    .handle_close  = sctp_client_close,
    .handle_block  = sctp_client_block,
};

void sctp_socks_accept(struct selector_key *key) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    sctp_client_t * state = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);

    if(client == -1){
        return;
    }

    if(selector_fd_set_nio(client) == -1){
        return;
    }


    state = sctp_client_new(client);
    if(state == NULL){
        goto fail;
    }

    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    if(selector_register(key->s, client, &client_handler, OP_READ, state) != SELECTOR_SUCCESS){
        goto fail;
    }

    return;
fail:
    if(client != -1) {
        close(client);
    }
    sctp_client_destroy(state);
}

int sctp_request_parser(char * read_buffer, char * write_buffer, int n) {
	int i, read_pos = 0, write_pos = 0;
	char metric[METRIC_SIZE], type, mediaType[MEDIATYPE_SIZE];
	
	for(i=0; i<PASSWORD_SIZE; i++) {
		if(password[i] != read_buffer[read_pos++]) {
			return 0;
		}
	}


	while(read_pos < n) {
		switch(read_buffer[read_pos++]) {
			case METRIC:
                type = read_buffer[read_pos++];
				bzero(metric, sizeof(metric));
				getMetric(type, metric);
				if(metric != 0) {
					write_buffer[write_pos++] = METRIC; //escribe que es una metrica
					write_buffer[write_pos++] = type; //escribe que tipo de metrica					
					write_buffer[write_pos++] = metric[0];
					write_buffer[write_pos++] = metric[1];
					write_buffer[write_pos++] = metric[2];
					write_buffer[write_pos++] = metric[3];
				}
				else {
					write_buffer[write_pos++] = METRIC_ERROR;
					write_buffer[write_pos++] = read_buffer[read_pos++];
				}
			break;
			case CONFIGURATION:
                type = read_buffer[read_pos++];
                bzero(mediaType, MEDIATYPE_SIZE);
                i = 0;
                while(read_buffer[read_pos] != ' ') {
                    mediaType[i++] = read_buffer[read_pos++];
                }
                mediaType[i] = 0;
                read_pos++;
				if(applyFilter(type, mediaType)) {
					write_buffer[write_pos++] = CONFIGURATION; //escribe que es una configuracion
					write_buffer[write_pos++] = type; //escribe que tipo de configuracion
				}
				else {
					write_buffer[write_pos++] = CONFIGURATION_ERROR;
					write_buffer[write_pos++] = type;
				}
			break;
		}
	}

	return 1;
}

void getMetric(char type, char * metric) {
  switch(type) {
    case '1':
      strcpy(metric, "0010");
    break;
    case '2':
      strcpy(metric, "0020");
    break;
    case '3':
      strcpy(metric, "0030");
    break;
  }
}

int applyFilter(char type, char * mediaType) {
  return 1;
}
