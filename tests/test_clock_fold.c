#include "../firmware/Board/clock_fold.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    desk_clock_fold clock={0,0};
    /* More than an hour with 1ms SysTick observations, sparse foreground reads. */
    for(uint32_t ms=1;ms<=3600000;ms++) {
        uint32_t us=ms*1000u;
        assert(desk_clock_observe(&clock,(uint16_t)us)==us);
        if(ms%117==0) assert(desk_clock_observe(&clock,(uint16_t)(us+83))==us+83);
    }
    clock=(desk_clock_fold){65530,0xfffffff0u};
    assert(desk_clock_observe(&clock,18)==8);
    clock=(desk_clock_fold){0,0};
    assert(desk_clock_observe(&clock,60000)==60000);
    assert(desk_clock_observe(&clock,61000)==61000);
    assert(desk_clock_observe(&clock,1000)==66536);
    puts("clock fold PASS: 3.6 million service samples, counter/uint32 wrap, bounded 60ms gap");
    return 0;
}
