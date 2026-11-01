#include "wokwi-api.h"
static pin_t sda; static timer_t timer; static unsigned stage;
static void tick(void *unused) { (void)unused; if(stage==0) { pin_mode(sda, OUTPUT_LOW); stage=1; timer_start(timer, 5000000, false); } else { pin_mode(sda, INPUT); stage=2; } }
void chip_init(void) { sda=pin_init("SDA",INPUT); const timer_config_t c={.callback=tick}; timer=timer_init(&c); stage=0; timer_start(timer,15000000,false); }
