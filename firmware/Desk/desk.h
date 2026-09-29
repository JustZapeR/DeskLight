#ifndef DESKLIGHT_DESK_H
#define DESKLIGHT_DESK_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { uint32_t last; uint8_t successes; bool valid, fault; } channel_health;
typedef enum { DESK_IDLE, DESK_ACTIVE, DESK_ADJUST } desk_mode;
typedef struct {
    channel_health pir_health, light_health, imu_health;
    desk_mode mode;
    bool pir, activity_seen, dark, need_light, tilt, fault, light_valid, angle_valid;
    uint32_t last_high;
    uint16_t raw, window[5];
    uint8_t count, cursor;
    uint32_t sum;
    float average, angle, norm;
} desk;

void desk_init(desk *state, uint32_t now);
void desk_pir(desk *state, bool level, uint32_t now);
void desk_light(desk *state, bool success, uint16_t raw, uint32_t now);
void desk_update(desk *state, uint32_t now);
void desk_auto(desk *state);
void desk_adjust(desk *state);
const char *desk_mode_name(desk_mode mode);
void channel_sample(channel_health *health, bool success, uint32_t now);
#endif
