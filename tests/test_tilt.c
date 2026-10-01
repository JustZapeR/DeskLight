#include "../firmware/Desk/desk.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void at(desk *d,uint32_t t,float angle,float magnitude)
{
    float a[3]={sinf(angle*.01745329252f)*magnitude,0,cosf(angle*.01745329252f)*magnitude};
    desk_pir(d,false,t); desk_light(d,true,2200,t); desk_imu(d,true,a,t); desk_update(d,t);
}
int main(void)
{
    desk d; desk_init(&d,0);
    for(unsigned t=0;t<=1000;t+=100) at(&d,t,19,1);
    assert(!d.tilt && d.angle_valid);
    at(&d,1100,20,1); at(&d,1500,20,1); assert(!d.tilt);
    at(&d,1599,20,1); assert(!d.tilt); at(&d,1600,20,1); assert(d.tilt);
    for(unsigned t=1700;t<=2300;t+=100) at(&d,t,16,1);
    assert(d.tilt && d.mode==DESK_IDLE);
    at(&d,2400,15,1); at(&d,2899,15,1); assert(d.tilt);
    at(&d,2900,15,1); assert(!d.tilt);
    at(&d,3000,21,1); at(&d,3100,21,1.5f);
    assert(!d.angle_valid && !d.confirming && !d.fault);
    at(&d,3200,21,1); at(&d,3600,21,1); assert(!d.tilt);
    at(&d,3700,21,1); assert(d.tilt);
    at(&d,3800,0,.9f); assert(d.angle_valid);
    at(&d,3900,0,1.1f); assert(d.angle_valid);
    at(&d,4000,0,.899f); assert(!d.angle_valid);
    at(&d,4100,0,1.101f); assert(!d.angle_valid);
    at(&d,4200,0,NAN); assert(!d.angle_valid);
    at(&d,4300,30,1); assert(!desk_zero(&d,4300));
    desk_adjust(&d); assert(!desk_zero(&d,4300));
    for(unsigned t=4400;t<=5300;t+=100) at(&d,t,30,1);
    assert(!desk_zero(&d,5399));
    at(&d,5400,30,1); assert(desk_zero(&d,5400));
    assert(!d.tilt && d.angle==0 && fabsf(d.reference[0]-.5f)<.0001f);
    at(&d,5500,50,1); at(&d,6000,50,1); assert(d.tilt);
    at(&d,6100,40,1); at(&d,6600,40,1); assert(!d.tilt);
    at(&d,6700,35,1); at(&d,6800,38,1); assert(!desk_zero(&d,6800));
    at(&d,6900,38,1); at(&d,7400,38,1); assert(!desk_zero(&d,7400));
    at(&d,7500,38,1); at(&d,7800,38,1); assert(desk_zero(&d,7800));
    desk_imu(&d,false,0,7801); desk_update(&d,7801);
    assert(d.fault && !d.angle_valid && !d.stable);
    at(&d,7900,0,1); assert(d.fault && !d.angle_valid);
    at(&d,8000,0,1); assert(!d.fault && d.angle_valid);
    desk_update(&d,8500); assert(d.angle_valid);
    desk_update(&d,8501); assert(d.fault && !d.angle_valid && !d.confirming);
    desk_init(&d,0xffffff00u); desk_adjust(&d);
    for(unsigned i=0;i<=1200;i+=100) at(&d,0xffffff00u+i,20,1);
    assert(d.tilt && desk_zero(&d,0xffffff00u+1200));
    assert(strcmp(desk_mode_name(DESK_ADJUST),"adjust")==0);
    puts("PASS tilt: 20/15 degrees, 499/500 ms, norm bounds, stability, zero, failures, stale, wrap");
}
