#include "sctpRequest.h"

#define CLIENT_ATTACHMENT(key) ( (sctp_client_t *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))

static char password[] = "tpprotos";

static void sctp_request_init(const unsigned state, struct selector_key *key) {
    sctp_request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

    d->read_buffer = CLIENT_ATTACHMENT(key)->read_buffer;
    d->write_buffer = CLIENT_ATTACHMENT(key)->write_buffer;
}

static unsigned sctp_request_read(struct selector_key *key) {
    sctp_request_st * d = &CLIENT_ATTACHMENT(key)->client.request;
    ssize_t n;

    bzero(d->read_buffer, MAX_BUFFER_SIZE);
    n = recv(key->fd, (void *) d->read_buffer, (size_t) MAX_BUFFER_SIZE, 0);
    if(n > 0) {
    	sctp_request_parser(d->read_buffer, d->write_buffer, n);
        selector_set_interest_key(key, OP_WRITE);
    	return SCTP_WRITE;
    } else {
    	return SCTP_ERROR;
    }
}

static void sctp_request_read_close(const unsigned state, struct selector_key *key) {

}

static unsigned sctp_request_write(struct selector_key *key) {
    sctp_request_st * d = &CLIENT_ATTACHMENT(key)->client.request;

    ssize_t n = send(key->fd, (void *) d->write_buffer, strlen((char*)d->write_buffer), MSG_NOSIGNAL);
    if(n == -1) {
        return SCTP_ERROR;
    }

    selector_set_interest_key(key, OP_NOOP);
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

void sctp_request_parser(uint8_t * read_buffer, uint8_t * write_buffer, int n) {
	int i, read_pos = 0, write_pos = 0;
	uint8_t metric[METRIC_SIZE + 1], type, mediaType[MEDIATYPE_SIZE];
	
	for(i=0; i<strlen(password); i++) {
		if(password[i] != read_buffer[read_pos++]) {
		    write_buffer[write_pos] = PASSWORD;
            return;
		}
	}


	while(read_pos < n) {
		switch(read_buffer[read_pos++]) {
			case METRIC:
                type = read_buffer[read_pos++];
				bzero(metric, METRIC_SIZE + 1);
				if(get_metric((metric_t) type-'0'-1, metric, METRIC_SIZE + 1)>= 0) {
				    write_buffer[write_pos++] = METRIC;
					write_buffer[write_pos++] = type;
					write_buffer[write_pos] = 0;
					strncat((char*)write_buffer, (char*)metric, METRIC_SIZE);
                    write_pos += METRIC_SIZE;
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
				write_buffer[write_pos++] = CONFIGURATION; //escribe que es una configuracion
				write_buffer[write_pos++] = type; //escribe que tipo de configuracion
                if(type == '0') {
                    unregister_transformation(mediaType);
                } else {
                    register_transformation(mediaType, (transformation_type_t) type - '0' - 1);
                }
			break;
		}
	}
}