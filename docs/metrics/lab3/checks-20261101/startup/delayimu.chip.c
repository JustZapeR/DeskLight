#include "wokwi-api.h"
static bool ready, writing_reg;
static uint8_t reg, regs[128];
static pin_t marker;
static bool connect_cb(void *u,uint32_t a,bool read) { (void)u; (void)a; if (!ready) return false; if (!read) writing_reg=true; return true; }
static bool write_cb(void *u,uint8_t value) { (void)u; if (writing_reg) {reg=value; writing_reg=false;} else {regs[reg++ & 127]=value;} return true; }
static uint8_t read_cb(void *u) { (void)u; return regs[reg++ & 127]; }
static void ready_cb(void *u) { (void)u; ready=true; pin_write(marker,1); }
void chip_init(void) { regs[0x75]=0x68; regs[0x3f]=0x40; marker=pin_init("READY",OUTPUT_LOW); pin_t scl=pin_init("SCL",INPUT_PULLUP),sda=pin_init("SDA",INPUT_PULLUP); const i2c_config_t i={.address=0x68,.scl=scl,.sda=sda,.connect=connect_cb,.write=write_cb,.read=read_cb}; i2c_init(&i); const timer_config_t t={.callback=ready_cb}; timer_t timer=timer_init(&t); timer_start(timer,3000000,false); }
