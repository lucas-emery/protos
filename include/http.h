#ifndef PC_2018_07_HTTP_H
#define PC_2018_07_HTTP_H

#include "selector.h"
#include "buffer.h"

bool send_http_code(unsigned code, struct selector_key * key);

#endif //PC_2018_07_HTTP_H
