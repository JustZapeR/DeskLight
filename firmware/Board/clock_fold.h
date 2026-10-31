#ifndef DESKLIGHT_CLOCK_FOLD_H
#define DESKLIGHT_CLOCK_FOLD_H
#include <stdint.h>
/* At least one observation per 65536 us is required. No UIF/NVIC ordering assumption. */
typedef struct { uint16_t last; uint32_t elapsed; } desk_clock_fold;
static inline uint32_t desk_clock_observe(desk_clock_fold *clock,uint16_t counter)
{
    clock->elapsed+=(uint16_t)(counter-clock->last);
    clock->last=counter;
    return clock->elapsed;
}
#endif
