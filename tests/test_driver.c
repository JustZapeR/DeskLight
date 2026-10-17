#include "../firmware/Sensors/imu.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "fixtures/mpu-captured.h"

typedef struct { unsigned calls, fail; io_result failure; uint8_t id; bool captured; } fake;
static io_result read_register(void *context, uint8_t address, uint8_t reg, uint8_t *data, size_t n)
{
    fake *f = context;
    assert(address == 0x68);
    if (++f->calls == f->fail) return f->failure;
    if (reg == 0x75) { assert(n == 1); *data = f->id; }
    else {
        static const uint8_t raw[14] = {0x80,0,0x7f,0xff,0x40,0,0xef,0x5c,0xff,0x7d,0,0x83,0,0};
        assert(reg == 0x3b && n == 14); memcpy(data, f->captured ? captured_mpu : raw, 14);
    }
    return IO_OK;
}
static io_result write_register(void *context, uint8_t address, uint8_t reg, uint8_t value)
{
    fake *f = context;
    static const uint8_t expected[][2] = {{0x6b,0},{0x1a,3},{0x1b,0},{0x1c,0},{0x19,9}};
    assert(address == 0x68 && f->calls >= 1 && f->calls <= 5);
    assert(reg == expected[f->calls-1][0] && value == expected[f->calls-1][1]);
    return ++f->calls == f->fail ? f->failure : IO_OK;
}
int main(void)
{
    fake f = {.id=0x68};
    imu_device d = {{&f,read_register,write_register},0x68};
    uint8_t id = 0, bytes[14];
    assert(imu_start(&d,&id) == IO_OK && id == 0x68 && f.calls == 6);
    for (unsigned result = IO_ERROR; result <= IO_TIMEOUT; ++result)
        for (unsigned fail = 1; fail <= 6; ++fail) {
            f = (fake){.id=0x68,.fail=fail,.failure=(io_result)result};
            assert(imu_start(&d,&id) == (io_result)result && f.calls == fail);
        }
    f = (fake){.id=0x69};
    assert(imu_start(&d,&id) == IO_ID && f.calls == 1);
    f = (fake){0};
    imu_sample sample;
    assert(imu_read(&d,&sample,bytes) == IO_OK);
    assert(sample.raw[0] == -32768 && sample.raw[1] == 32767);
    assert(sample.acceleration[0] == -2 && sample.acceleration[2] == 1);
    assert(sample.rate[0] == -1 && sample.rate[1] == 1 && sample.rate[2] == 0);
    assert(fabsf(sample.temperature - 24) < .002f);
    imu_sample before = sample; uint8_t before_bytes[14]; memcpy(before_bytes,bytes,14);
    for (unsigned result=IO_ERROR;result<=IO_TIMEOUT;++result) {
        f=(fake){.fail=1,.failure=(io_result)result};
        assert(imu_read(&d,&sample,bytes)==(io_result)result);
        assert(memcmp(&before,&sample,sizeof sample)==0 && memcmp(before_bytes,bytes,14)==0);
    }
    f=(fake){.captured=true};
    assert(imu_read(&d,&sample,bytes)==IO_OK);
    assert(memcmp(bytes,captured_mpu,14)==0);
    assert(sample.raw[0]==-1638 && sample.raw[2]==15564);
    assert(fabsf(sample.acceleration[0]-(-1638.0f/16384))<1e-7f);
    assert(fabsf(sample.acceleration[2]-(15564.0f/16384))<1e-7f);
    assert(fabsf(sample.temperature-24)<.002f);
    assert(sample.rate[0]==0 && sample.rate[1]==0 && sample.rate[2]==0);
    assert(strcmp(io_name(IO_ID),"BAD_ID")==0);
    assert(strcmp(io_name((io_result)99),"ERROR")==0);
    puts("PASS driver: ID, configuration order, errors, signed units, unchanged failed output, actual captured burst");
    return 0;
}
