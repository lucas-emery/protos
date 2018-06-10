#include <stdlib.h>
#include "metrics.h"
#include <string.h>

#define KB 1024
#define US2MS 1000
#define S2US 1000*1000
#define FORMAT "%-46s\t%-16lu\n" //64 byte string

typedef struct {
    size_t round;
    size_t carry;
    metric_t type;
    int cos;
    char * string;
} pack_t;

pack_t metrics[] = {
        {
            .round  = 0,
            .carry  = 0,
            .type   = TRAFFIC,
            .cos    = KB,
            .string = "KB TRANSFERRED:"
        },{
            .round  = 0,
            .carry  = 0,
            .type   = CLIENTS,
            .cos    = 1,
            .string = "REQUESTS SERVED:"
        },{
            .round  = 0,
            .carry  = 0,
            .type   = DNS,
            .cos    = US2MS,
            .string = "TIME ELAPSED RESOLVING:"
        },{
            .round  = 0,
            .carry  = 0,
            .type   = CONN,
            .cos    = US2MS,
            .string = "TIME ELAPSED CONNECTING:"
        },{
            .round  = 0,
            .carry  = 0,
            .type   = REQUEST,
            .cos    = US2MS,
            .string = "TIME ELAPSED TRANSFERRING REQUEST:"
        },{
            .round  = 0,
            .carry  = 0,
            .type   = RESPONSE,
            .cos    = US2MS,
            .string = "TIME ELAPSED TRANSFERRING RESPONSE:"
        },{
            .round  = 0,
            .carry  = 0,
            .type   = TRANSFORMING,
            .cos    = US2MS,
            .string = "TIME ELAPSED TRANSFORMING:"
        }
};

static void log_util(pack_t * pack, size_t n){
    pack->carry += n%pack->cos;
    pack->round += n/pack->cos;
    pack->round += pack->carry/pack->cos;
    pack->carry  = pack->carry%pack->cos;
}

void log(metric_t type, size_t n){
    log_util(&metrics[type], n);
}

int get_metric(metric_t type, uint8_t * buffer, size_t size){
    char aux[KB];
    snprintf(aux, KB, FORMAT, metrics[type].string, metrics[type].round);
    if(strlen(aux) > size)
        return -1;
    strcpy(buffer, aux);
    return 0;
}

void startTimer(struct timeval * t){
    gettimeofday(t, NULL);
}

void logTime(metric_t type, struct timeval * t){
    struct timeval now;
    gettimeofday(&now, NULL);

    if(type == DNS)
        ;

    size_t deltaSec = now.tv_sec - t->tv_sec;

    log(type, deltaSec*S2US + now.tv_usec - t->tv_usec);
}



