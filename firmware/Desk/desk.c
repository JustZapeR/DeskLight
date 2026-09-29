#include "desk.h"
#include <string.h>

void channel_sample(channel_health *health, bool success, uint32_t now)
{
    if (!success) {
        health->successes = 0; health->valid = false; health->fault = true;
        return;
    }
    health->last = now;
    if (health->successes < 2) ++health->successes;
    if (health->successes == 2) { health->valid = true; health->fault = false; }
}

static void expire(channel_health *health, uint32_t now)
{
    if ((uint32_t)(now - health->last) > 500) channel_sample(health, false, now);
}

void desk_init(desk *state, uint32_t now)
{
    memset(state, 0, sizeof *state);
    state->pir_health.last = state->light_health.last = state->imu_health.last = now;
}

void desk_pir(desk *state, bool level, uint32_t now)
{
    channel_sample(&state->pir_health, true, now);
    state->pir = level;
    if (level && state->mode != DESK_ADJUST) {
        state->last_high = now; state->activity_seen = true;
    }
}

void desk_light(desk *state, bool success, uint16_t raw, uint32_t now)
{
    if ((uint32_t)(now-state->light_health.last) > 500) {
        state->count = state->cursor = 0; state->sum = 0;
    }
    channel_sample(&state->light_health, success && raw <= 4095, now);
    if (!success || raw > 4095) return;
    state->raw = raw;
    if (state->count == 5) state->sum -= state->window[state->cursor];
    else ++state->count;
    state->window[state->cursor] = raw; state->sum += raw;
    state->cursor = (state->cursor+1) % 5;
    state->average = state->sum / 5.0f;
    if (state->count == 5) {
        if (state->average >= 2400) state->dark = true;
        else if (state->average <= 2000) state->dark = false;
    }
}

void desk_update(desk *state, uint32_t now)
{
    expire(&state->pir_health, now); expire(&state->light_health, now);
    expire(&state->imu_health, now);
    if (state->pir_health.fault) state->activity_seen = false;
    if (state->mode != DESK_ADJUST)
        state->mode = state->activity_seen && (uint32_t)(now-state->last_high)<30000
                      ? DESK_ACTIVE : DESK_IDLE;
    state->light_valid = state->light_health.valid && state->count == 5;
    state->need_light = state->mode == DESK_ACTIVE && state->light_valid && state->dark;
    state->fault = state->pir_health.fault || state->light_health.fault || state->imu_health.fault;
}

void desk_auto(desk *state) { state->mode = DESK_IDLE; state->activity_seen = false; state->need_light = false; }
void desk_adjust(desk *state) { state->mode = DESK_ADJUST; state->need_light = false; }
const char *desk_mode_name(desk_mode mode)
{
    return mode == DESK_ADJUST ? "adjust" : mode == DESK_ACTIVE ? "active" : "idle";
}
