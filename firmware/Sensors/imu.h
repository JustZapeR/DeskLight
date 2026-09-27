#ifndef DESKLIGHT_IMU_H
#define DESKLIGHT_IMU_H
#include <stddef.h>
#include <stdint.h>

typedef enum { IO_OK, IO_ERROR, IO_TIMEOUT, IO_ID } io_result;
typedef struct {
    void *context;
    io_result (*read)(void *, uint8_t, uint8_t, uint8_t *, size_t);
    io_result (*write)(void *, uint8_t, uint8_t, uint8_t);
} register_bus;
typedef struct {
    int16_t raw[7];
    float acceleration[3];
    float rate[3];
    float temperature;
} imu_sample;
typedef struct { register_bus bus; uint8_t address; } imu_device;

io_result imu_start(imu_device *device, uint8_t *id);
io_result imu_read(imu_device *device, imu_sample *sample, uint8_t bytes[14]);
void imu_decode(const uint8_t bytes[14], imu_sample *sample);
const char *io_name(io_result result);
#endif
