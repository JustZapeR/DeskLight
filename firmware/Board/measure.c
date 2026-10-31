#include "board.h"
#include "measure.h"
#include <limits.h>
#include "systick_time.h"
void measure_begin(void) { }
uint32_t measure_us(void)
{
    return desk_milliseconds_to_us(HAL_GetTick());
}
void measure_add(task_meter *m, uint32_t us)
{
    if(!m->n || us<m->min) m->min=us;
    if(!m->n || us>m->max) m->max=us;
    m->n++; m->sum+=us;
}
void measure_release(task_meter *m, uint32_t now, uint32_t period)
{
    if(m->n) {
        uint32_t delta=now-m->last;
        int32_t jitter=(int32_t)delta-(int32_t)period;
        if(m->n==1 || jitter<m->jitter_min) m->jitter_min=jitter;
        if(m->n==1 || jitter>m->jitter_max) m->jitter_max=jitter;
    }
    m->last=now;
    uint32_t late=now-m->due;
    if(late>=period) { m->missed+=late/period; m->deadline_misses+=late/period; }
    m->due+=(late/period+1)*period;
}
extern uint32_t _estack, _Min_Stack_Size;
void measure_stack_paint(void)
{
    uint32_t *bottom=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    uint32_t *safe=(uint32_t *)__get_MSP()-32;
    while(bottom<safe) *bottom++=0xd35b71a9u;
}
uint32_t measure_stack_used(void)
{
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    while(p<&_estack && *p==0xd35b71a9u) p++;
    return (uint32_t)((uintptr_t)&_estack-(uintptr_t)p);
}
