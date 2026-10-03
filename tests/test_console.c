#include "../firmware/Desk/console.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static desk_command line(desk_console *c,const char *s)
{
    desk_command command=CMD_NONE;
    while(*s) command=console_feed(c,*s++);
    return command;
}
int main(void)
{
    desk_console c={0}; desk d; desk_init(&d,0);
    assert(line(&c,"mode adjust\r")==CMD_ADJUST);
    assert(console_feed(&c,'\n')==CMD_NONE);
    assert(strcmp(console_apply(&d,CMD_ADJUST,0),"mode adjust ok")==0);
    assert(d.mode==DESK_ADJUST);
    assert(strcmp(console_apply(&d,CMD_ZERO,0),"zero rejected")==0);
    assert(line(&c,"mode auto\n")==CMD_AUTO);
    console_apply(&d,CMD_AUTO,0); assert(d.mode==DESK_IDLE);
    assert(line(&c,"zero\n")==CMD_ZERO);
    assert(line(&c,"sample\n")==CMD_SAMPLE);
    assert(line(&c,"probe nack\n")==CMD_NACK);
    assert(line(&c,"fault imu on\n")==CMD_IMU_OFF);
    assert(line(&c,"fault adc off\n")==CMD_ADC_ON);
    assert(line(&c,"pause imu on\n")==CMD_IMU_PAUSE);
    assert(line(&c,"bad\n")==CMD_UNKNOWN);
    assert(strcmp(console_apply(&d,CMD_UNKNOWN,0),"unknown command")==0);
    for(unsigned i=0;i<100;++i) console_feed(&c,'x');
    assert(console_feed(&c,'\n')==CMD_UNKNOWN && line(&c,"zero\n")==CMD_ZERO);
    desk_led led={0};
    assert(!led_update(&led,&d,0));
    d.need_light=true; assert(led_update(&led,&d,1));
    d.tilt=true; assert(led_update(&led,&d,2));
    assert(led_update(&led,&d,251) && !led_update(&led,&d,252) && led_update(&led,&d,502));
    d.fault=true; assert(led_update(&led,&d,600));
    assert(led_update(&led,&d,699) && !led_update(&led,&d,700) && led_update(&led,&d,800));
    char status[256]; assert(desk_status(&d,800,status,sizeof status)>0);
    assert(strstr(status,"need_light=1 tilt=1 fault=1") && strstr(status,"angle=invalid"));
    assert(desk_status(&d,800,status,2)==0);
    d.fault=false; d.tilt=false; assert(led_update(&led,&d,900));
    d.need_light=false; assert(!led_update(&led,&d,1000));
    d.fault=true; assert(led_update(&led,&d,0xffffff00u));
    assert(!led_update(&led,&d,0xffffff00u+100) && led_update(&led,&d,0xffffff00u+200));
    puts("PASS console: commands, CRLF, overflow, LED periods and priority, concurrent UART flags");
}
