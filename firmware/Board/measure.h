#ifndef DESKLIGHT_MEASURE_H
#define DESKLIGHT_MEASURE_H
#include <stdint.h>
typedef struct {
    uint32_t n, min, max, missed, deadline_misses, last, due;
    int32_t jitter_min, jitter_max;
    uint64_t sum;
} task_meter;
void measure_begin(void);
uint32_t measure_us(void);
void measure_add(task_meter *, uint32_t);
void measure_release(task_meter *, uint32_t, uint32_t);
void measure_stack_paint(void);
uint32_t measure_stack_used(void);
#endif
