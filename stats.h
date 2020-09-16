#pragma once


typedef enum {
    STATS_MEASURE_STOP = 0,
    STATS_MEASURE_START
} stats_measure_state_t;

typedef struct {
    char name[16];
    int64_t time;
    int64_t start;
    stats_measure_state_t state;
} stats_run_time_t;

void stats_init(void);
void stats_reset_accumulated_infos(void);

stats_run_time_t *stats_run_time_init(const char *name);
void stats_run_time_start(stats_run_time_t *handler);
void stats_run_time_stop(stats_run_time_t *handler);
void stats_run_time_free(stats_run_time_t *handler);
void stats_run_time_print(const stats_run_time_t *handler);