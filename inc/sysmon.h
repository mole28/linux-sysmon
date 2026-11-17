#ifndef SYSMON_H
#define SYSMON_H

#include <pthread.h>

typedef struct
{
    long mem_total;
    long mem_free;
    long mem_avail;
    long swap_total;
    long swap_free;

    double load1;
    double load5;
    double load15;

    int total_procs;

    pthread_mutex_t lock;
    pthread_cond_t alert_cond;
} SystemState;

void *mem_sensor_thread(void *arg);
void *cpu_sensor_thread(void *arg);
void *proc_sensor_thread(void *arg);
void *alert_thread(void *arg);

#endif
