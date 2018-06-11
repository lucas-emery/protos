#include "resolveLocalIp.h"

#define IP_MAX 100

struct sockaddr_in* local_ips[IP_MAX];
int local_ips_count = 0;

void
local_ip_resolv(int port_number) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        free(ifaddr);
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL && local_ips_count < IP_MAX; ifa = ifa->ifa_next) {
        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            struct sockaddr_in* current = (struct sockaddr_in*)ifa->ifa_addr;
            current->sin_port = htons(port_number);
            local_ips[local_ips_count] = malloc(sizeof(struct sockaddr_in));
            memcpy(local_ips[local_ips_count++], current, sizeof(struct sockaddr_in));
        }
    }

    free(ifaddr);

    if(local_ips_count >= IP_MAX) {
        DieWithSystemMessage("Too many local ips");
    }
}

void
free_ips() {
    for (int i = 0; i < local_ips_count; ++i) {
        free(local_ips[i]);
    }
}

bool
check_local_ip(struct sockaddr_storage* origin_addr) {
    struct sockaddr_in* origin= (struct sockaddr_in*) origin_addr;

    for (int i = 0; i < local_ips_count; ++i) {
        struct sockaddr_in* current = local_ips[i];

        if((current->sin_addr.s_addr == origin->sin_addr.s_addr) && (current->sin_port == origin->sin_port))
            return true;
    }
    return false;
}
