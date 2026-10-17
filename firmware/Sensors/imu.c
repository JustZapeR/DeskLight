#include "imu.h"

static int16_t signed_word(const uint8_t *p)
{
    uint16_t word = ((uint16_t)p[0] << 8) | p[1];
    return (int16_t)(word < 32768u ? (int32_t)word : (int32_t)word - 65536);
}

void imu_decode(const uint8_t bytes[14], imu_sample *sample)
{
    for (unsigned i = 0; i < 7; ++i) sample->raw[i] = signed_word(bytes + 2 * i);
    for (unsigned i = 0; i < 3; ++i) {
        sample->acceleration[i] = sample->raw[i] / 16384.0f;
        sample->rate[i] = sample->raw[i + 4] / 131.0f;
    }
    sample->temperature = sample->raw[3] / 340.0f + 36.53f;
}

io_result imu_start(imu_device *device, uint8_t *id)
{
    io_result result = device->bus.read(device->bus.context, device->address, 0x75, id, 1);
    if (result != IO_OK) return result;
    if (*id != 0x68) return IO_ID;
    /* Wake, DLPF 44 Hz, +/-250 deg/s, +/-2 g, internal sample 100 Hz. */
    static const uint8_t settings[][2] = {
        {0x6b, 0x00}, {0x1a, 0x03}, {0x1b, 0x00}, {0x1c, 0x00}, {0x19, 0x09}
    };
    for (unsigned i = 0; i < sizeof settings / sizeof settings[0]; ++i) {
        result = device->bus.write(device->bus.context, device->address,
                                   settings[i][0], settings[i][1]);
        if (result != IO_OK) return result;
    }
    return IO_OK;
}

io_result imu_read(imu_device *device, imu_sample *sample, uint8_t bytes[14])
{
    uint8_t frame[14];
    io_result result = device->bus.read(device->bus.context, device->address, 0x3b, frame, 14);
    if (result != IO_OK) return result;
    imu_decode(frame, sample);
    for (unsigned i = 0; i < 14; ++i) bytes[i] = frame[i];
    return IO_OK;
}

const char *io_name(io_result result)
{
    static const char *const names[] = {"OK", "ERROR", "TIMEOUT", "BAD_ID"};
    return names[(unsigned)result <= IO_ID ? (unsigned)result : IO_ERROR];
}
