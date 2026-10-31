#ifndef DESKLIGHT_SYSTICK_TIME_H
#define DESKLIGHT_SYSTICK_TIME_H
#include <stdint.h>
/* Quantized to 1 ms. Wraps every 2^32 microseconds. */
static inline uint32_t desk_milliseconds_to_us(uint32_t tick)
{
    return tick * 1000u;
}
#endif
