#ifndef DESKLIGHT_CONSOLE_H
#define DESKLIGHT_CONSOLE_H
#include "desk.h"
#include <stddef.h>
typedef enum {
    CMD_NONE, CMD_ADJUST, CMD_AUTO, CMD_ZERO, CMD_SAMPLE, CMD_NACK,
    CMD_IMU_OFF, CMD_IMU_ON, CMD_ADC_OFF, CMD_ADC_ON,
    CMD_IMU_PAUSE, CMD_IMU_RESUME, CMD_ADC_PAUSE, CMD_ADC_RESUME, CMD_UNKNOWN
} desk_command;
typedef struct { char line[40]; unsigned length; bool overflow; } desk_console;
typedef struct { unsigned priority; uint32_t changed; bool level; } desk_led;
desk_command console_feed(desk_console *console, char byte);
const char *console_apply(desk *state, desk_command command, uint32_t now);
bool led_update(desk_led *led, const desk *state, uint32_t now);
size_t desk_status(const desk *state, uint32_t now, char *output, size_t capacity);
#endif
