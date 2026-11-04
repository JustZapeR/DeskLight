#include "wokwi-api.h"
static pin_t pir; static timer_t timer; static unsigned stage;
static void tick(void *unused) { (void)unused; if(stage==0){pin_write(pir,HIGH);stage=1;timer_start(timer,60000,false);} else {pin_write(pir,LOW);stage=0;timer_start(timer,6953000,false);} }
void chip_init(void){pir=pin_init("PIR",OUTPUT_LOW);const timer_config_t c={.callback=tick};timer=timer_init(&c);timer_start(timer,20000000,false);}
