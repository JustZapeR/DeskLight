#include "../firmware/Board/systick_time.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    for(uint32_t i=0;i<3600000;i+=997) {
        uint32_t before=desk_milliseconds_to_us(i), after=desk_milliseconds_to_us(i+997);
        assert(after-before==997000u);
    }
    uint32_t before=desk_milliseconds_to_us(4294967u);
    uint32_t after=desk_milliseconds_to_us(4294968u);
    assert(after-before==1000u);
    before=desk_milliseconds_to_us(UINT32_MAX); after=desk_milliseconds_to_us(0);
    assert(after-before==1000u);
    puts("SysTick conversion PASS: one-hour timeline, 32-bit microsecond and tick wraps");
    return 0;
}
