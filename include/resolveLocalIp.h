//
// Created by parallels on 6/11/18.
//

#ifndef PC_2018_07_RESOLVELOCALIP_H
#define PC_2018_07_RESOLVELOCALIP_H

#include "message.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

void
local_ip_resolv(int port_number);

bool
check_local_ip(struct sockaddr_storage* origin_addr);

void
free_ips();

#endif //PC_2018_07_RESOLVELOCALIP_H
