#include <stdlib.h>
#include "metrics.h"
#include <string.h>
#include <sys/time.h>

#define KB 1024
#define US2MS 1000
#define S2US 1000*1000
#define FORMAT "%-42s\t%-16lu%4s\n" //64 byte string
#define KB_UNIT "(KB)"
#define MS_UNIT "(ms)"

typedef struct {
    size_t round;
    size_t carry;
    metric_t type;
    int cos;
    char * string;
    char * units;
} pack_t;

pack_t metrics[] = {
        {
            .round  = 0,
            .carry  = 0,
            .type   = TRAFFIC,
            .cos    = KB,
            .string = "KB TRANSFERRED:",
            .units  = KB_UNIT
        },{
            .round  = 0,
            .carry  = 0,
            .type   = CONNECTIONS,
            .cos    = 1,
            .string = "CURRENTLY SERVING:",
            .units  = ""
        },{
            .round  = 0,
            .carry  = 0,
            .type   = MAX_CONNECTIONS,
            .cos    = 1,
            .string = "MAX SERVED CONCURRENTLY:",
            .units  = ""
        },{
            .round  = 0,
            .carry  = 0,
            .type   = CLIENTS,
            .cos    = 1,
            .string = "REQUESTS SERVED:",
            .units  = ""
        },{
            .round  = 0,
            .carry  = 0,
            .type   = DNS,
            .cos    = US2MS,
            .string = "TIME ELAPSED RESOLVING:",
            .units  = MS_UNIT
        },{
            .round  = 0,
            .carry  = 0,
            .type   = CONN,
            .cos    = US2MS,
            .string = "TIME ELAPSED CONNECTING:",
            .units  = MS_UNIT
        },{
            .round  = 0,
            .carry  = 0,
            .type   = REQUEST,
            .cos    = US2MS,
            .string = "TIME ELAPSED TRANSFERRING REQUEST:",
            .units  = MS_UNIT
        },{
            .round  = 0,
            .carry  = 0,
            .type   = RESPONSE,
            .cos    = US2MS,
            .string = "TIME ELAPSED TRANSFERRING RESPONSE:",
            .units  = MS_UNIT
        },{
            .round  = 0,
            .carry  = 0,
            .type   = TRANSFORMING,
            .cos    = US2MS,
            .string = "TIME ELAPSED TRANSFORMING:",
            .units  = MS_UNIT
        }
};

static void log_util(pack_t * pack, size_t n){
    pack->carry += n%pack->cos;
    pack->round += n/pack->cos;
    pack->round += pack->carry/pack->cos;
    pack->carry  = pack->carry%pack->cos;
}

void log_metric(metric_t type, size_t n){
    log_util(&metrics[type], n);
}

int get_metric(metric_t type, uint8_t * buffer, size_t size){
    char aux[KB];
    snprintf(aux, KB, FORMAT, metrics[type].string, metrics[type].round, metrics[type].units);
    if(strlen(aux) > size)
        return -1;
    strcpy((char*)buffer, aux);
    return 0;
}

void startTimer(struct timeval * t){
    gettimeofday(t, NULL);
}

void logTime(metric_t type, struct timeval * t){
    struct timeval now, res;
    gettimeofday(&now, NULL);
    timersub(&now,t,&res);
    log_metric(type, (res.tv_sec * S2US) + res.tv_usec);
}

void add_client(){
    metrics[CONNECTIONS].round++;
    if(metrics[MAX_CONNECTIONS].round < metrics[CONNECTIONS].round)
        metrics[MAX_CONNECTIONS].round = metrics[CONNECTIONS].round;
}

void remove_client(){
    if(metrics[CONNECTIONS].round > 0)
        metrics[CONNECTIONS].round--;
}


