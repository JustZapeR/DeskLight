#include "../firmware/Desk/console.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

static void poll(desk *d,uint32_t t,bool pir,uint16_t raw,bool light_ok,bool imu_ok,float angle)
{
    float a[3]={sinf(angle*.01745329252f),0,cosf(angle*.01745329252f)};
    desk_pir(d,pir,t); desk_light(d,light_ok,raw,t);
    desk_imu(d,imu_ok,a,t); desk_update(d,t);
}
int main(void)
{
    desk d; desk_init(&d,0);
    poll(&d,20,true,3000,true,true,30);
    assert(d.mode==DESK_ACTIVE); /* The first HIGH must survive startup recovery. */
    for(unsigned t=100;t<=800;t+=100) poll(&d,t,true,3000,true,true,30);
    assert(d.need_light && d.tilt && !d.fault);
    desk_led led={0}; assert(led_update(&led,&d,800) && led.priority==2);
    poll(&d,900,true,3000,false,false,30);
    assert(d.fault && d.tilt && !d.need_light && !d.angle_valid);
    assert(led_update(&led,&d,900) && led.priority==3);
    poll(&d,1000,true,3000,true,false,30);
    assert(!d.light_valid && d.fault);
    poll(&d,1100,true,3000,true,false,30);
    assert(d.light_valid && d.need_light && d.fault);
    poll(&d,1200,true,3000,true,true,30); assert(d.fault);
    poll(&d,1300,true,3000,true,true,30); assert(!d.fault && d.angle_valid);
    /* Fresh exactly at 500 ms, invalid at 501. */
    desk_update(&d,1800); assert(d.light_valid && d.angle_valid && !d.fault);
    desk_update(&d,1801); assert(!d.light_valid && !d.angle_valid && d.fault && d.mode==DESK_IDLE);
    poll(&d,1900,false,3000,true,true,0); assert(!d.light_health.valid);
    poll(&d,2000,false,3000,true,true,0);
    assert(d.light_health.valid && d.imu_health.valid && !d.fault && !d.light_valid);
    for(unsigned t=2100;t<=2500;t+=100) poll(&d,t,false,3000,true,true,0);
    assert(d.light_valid && !d.tilt && !d.need_light);
    /* Fractional averages cannot turn 2399.8 into 2400 by rounding. */
    desk_init(&d,0);
    for(unsigned i=0;i<5;++i) poll(&d,i*100,true,i==4?2399:2400,true,true,0);
    assert(d.average==2399.8f && !d.dark);
    poll(&d,500,true,2400,true,true,0); assert(!d.dark);
    for(unsigned t=600;t<=1000;t+=100) poll(&d,t,true,2400,true,true,0);
    assert(d.dark);
    desk_light(&d,true,4096,1100); desk_update(&d,1100);
    assert(d.fault && !d.need_light);
    /* Restart clears a user zero, modes, ring and activity. */
    desk_adjust(&d);
    for(unsigned t=1200;t<=2400;t+=100) poll(&d,t,true,2400,true,true,40);
    assert(desk_zero(&d,2400)); desk_init(&d,2500); desk_update(&d,2500);
    assert(d.reference[0]==0 && d.reference[2]==1 && d.mode==DESK_IDLE && d.count==0);
    /* More commands while all channels continue for ten seconds. */
    desk_console c={0};
    for(unsigned t=2600;t<=12600;t+=20) {
        poll(&d,t,true,3000,true,true,0);
        if(t%100==0) {
            const char *text=t%200==0?"mode adjust\n":"mode auto\n";
            while(*text) {
                desk_command command=console_feed(&c,*text++);
                if(command!=CMD_NONE) console_apply(&d,command,t);
            }
            desk_update(&d,t);
        }
    }
    assert(!d.fault);
    char status[256];
    d.angle_valid=true; d.angle=20.125f;
    assert(desk_status(&d,12600,status,sizeof status) && strstr(status,"angle=20.125"));
    puts("PASS faults: concurrent flags, independent two-sample recovery, 500/501 ms, fractional average, restart, concurrent commands");
    return 0;
}
