#include "../firmware/Desk/desk.h"
#include <assert.h>
#include <stdio.h>

static void healthy(desk *d, uint32_t now, bool pir, uint16_t raw)
{
    desk_pir(d,pir,now); desk_light(d,true,raw,now);
    channel_sample(&d->imu_health,true,now); desk_update(d,now);
}
static void fill(desk *d,uint32_t now,uint16_t raw)
{
    for(unsigned i=0;i<5;++i) healthy(d,now+i*100,true,raw);
}
int main(void)
{
    desk d; desk_init(&d,0); desk_update(&d,0);
    assert(d.mode==DESK_IDLE && !d.light_valid && !d.fault);
    for(unsigned i=0;i<4;++i) healthy(&d,i*100,false,2200);
    assert(!d.light_valid && !d.dark);
    healthy(&d,400,true,2200); assert(d.light_valid && d.mode==DESK_ACTIVE && !d.dark);
    fill(&d,500,2399); assert(!d.dark && d.average==2399);
    fill(&d,1000,2400); assert(d.dark && d.need_light);
    fill(&d,1500,2001); assert(d.dark);
    fill(&d,2000,2000); assert(!d.dark);
    fill(&d,2500,4095); assert(d.dark);
    fill(&d,3000,0); assert(!d.dark && d.average==0);
    for(unsigned i=1;i<=5;++i) healthy(&d,3500+100*i,true,i*500);
    assert(d.average==1500);
    for(uint32_t t=4100;t<34000;t+=100) healthy(&d,t,false,3000);
    assert(d.mode==DESK_ACTIVE);
    healthy(&d,34000,false,3000); assert(d.mode==DESK_IDLE);
    /* Every HIGH refreshes the timestamp, including a held level. */
    for(uint32_t t=35000;t<=40000;t+=20) healthy(&d,t,true,3000);
    assert(d.last_high==40000);
    for(uint32_t t=40100;t<=65000;t+=100) healthy(&d,t,false,3000);
    healthy(&d,65020,true,3000);
    for(uint32_t t=65100;t<95020;t+=100) healthy(&d,t,false,3000);
    assert(d.mode==DESK_ACTIVE);
    healthy(&d,95020,false,3000); assert(d.mode==DESK_IDLE);
    for(uint32_t t=95200;t<=96000;t+=100) healthy(&d,t,true,3000);
    desk_adjust(&d); desk_update(&d,96000);
    assert(d.mode==DESK_ADJUST && !d.need_light);
    desk_auto(&d); desk_update(&d,96000); assert(d.mode==DESK_IDLE && !d.activity_seen);
    desk_light(&d,false,0,96001); desk_update(&d,96001);
    assert(d.fault && !d.light_valid && !d.need_light);
    desk_light(&d,true,3000,96100); desk_update(&d,96100); assert(!d.light_valid);
    desk_light(&d,true,3000,96200); desk_update(&d,96200); assert(d.light_valid);
    desk_init(&d,0xffffff00u);
    for(uint32_t i=0;i<=400;++i) { if(i%20==0) healthy(&d,0xffffff00u+i,true,3000); }
    assert(d.mode==DESK_ACTIVE && !d.fault);
    puts("PASS desk: startup, inclusive light limits, rolling average, PIR extension, modes, recovery, wrap");
}
