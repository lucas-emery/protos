

static bool
read_request(const int fd, struct request *request) {
    uint8_t buff[22];
    buffer  buffer; buffer_init(&buffer, N(buff), buff);
    size_t  buffsize;

    struct request_parser request_parser = {
        .request = request,
    };
    request_parser_init(&request_parser);
    unsigned    n = 0;
       bool error = false;

    do {
        uint8_t *ptr = buffer_write_ptr(&buffer, &buffsize);
        n = recv(fd, ptr, buffsize, 0);
        if(n > 0) {
            buffer_write_adv(&buffer, n);
            const enum request_state st = request_consume(&buffer,
                        &request_parser, &error);
            if(request_is_done(st, &error)) {
                break;
            }

        } else {
            break;
        }
    }while(true);
    if(!request_is_done(request_parser.state, &error)) {
        error = true;
    }
    request_close(&request_parser);
    return error;
}
