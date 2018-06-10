#ifndef _METRICS
#define _METRICS

#include <time.h>
#include <stdint.h>

typedef enum {
    TRAFFIC,
    CLIENTS,
    DNS,
    CONN,
    REQUEST,
    RESPONSE,
    TRANSFORMING
} metric_t;

void log(metric_t type, size_t n);

int get_metric(metric_t type, uint8_t * buffer, size_t size);

void startTimer(struct timeval * t);

void logTime(metric_t type, struct timeval * t);

#endif