#include "desk.h"
#include <string.h>
#include <math.h>

static void invalidate_angle(desk *state)
{
    state->angle_valid = state->acceleration_valid = false;
    state->confirming = state->stable = false;
}

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
    state->reference[2] = 1;
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
        channel_sample(&state->light_health,false,now);
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
    if (!state->imu_health.valid) invalidate_angle(state);
    if (state->pir_health.fault) state->activity_seen = false;
    if (state->mode != DESK_ADJUST)
        state->mode = state->activity_seen && (uint32_t)(now-state->last_high)<30000
                      ? DESK_ACTIVE : DESK_IDLE;
    state->light_valid = state->light_health.valid && state->count == 5;
    state->need_light = state->mode == DESK_ACTIVE && state->light_valid && state->dark;
    state->fault = state->pir_health.fault || state->light_health.fault || state->imu_health.fault;
}

void desk_auto(desk *state) { state->mode = DESK_IDLE; state->activity_seen = false; state->need_light = false; }
void desk_adjust(desk *state)
{
    state->mode = DESK_ADJUST; state->need_light = false; state->stable = false;
}
const char *desk_mode_name(desk_mode mode)
{
    return mode == DESK_ADJUST ? "adjust" : mode == DESK_ACTIVE ? "active" : "idle";
}

static float vector_angle(const float a[3], const float b[3])
{
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    if (dot > 1) dot = 1;
    if (dot < -1) dot = -1;
    return acosf(dot) * 57.2957795131f;
}

void desk_imu(desk *state, bool success, const float a[3], uint32_t now)
{
    if ((uint32_t)(now-state->imu_health.last)>500)
        channel_sample(&state->imu_health,false,now);
    channel_sample(&state->imu_health,success,now);
    if (!success) { invalidate_angle(state); return; }
    state->norm = sqrtf(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
    if (!isfinite(state->norm) || state->norm < .9f-1e-6f || state->norm > 1.1f+1e-6f) {
        invalidate_angle(state); return;
    }
    for(unsigned i=0;i<3;++i) state->direction[i]=a[i]/state->norm;
    state->acceleration_valid = true;
    state->angle_valid = state->imu_health.valid;
    if (!state->angle_valid) { state->confirming=state->stable=false; return; }
    state->angle = vector_angle(state->direction,state->reference);
    bool qualifying = state->tilt ? state->angle <= 15.0f+1e-4f
                                  : state->angle >= 20.0f-1e-4f;
    if (!qualifying) state->confirming=false;
    else if (!state->confirming) { state->confirming=true; state->confirm_since=now; }
    else if ((uint32_t)(now-state->confirm_since)>=500) {
        state->tilt=!state->tilt; state->confirming=false;
    }
    const float vertical[3]={0,0,1};
    float polar=vector_angle(state->direction,vertical);
    if (!state->stable) {
        state->stable=true; state->stable_since=now;
        state->stable_min=state->stable_max=polar;
    } else {
        if (polar < state->stable_min) state->stable_min=polar;
        if (polar > state->stable_max) state->stable_max=polar;
        if (state->stable_max-state->stable_min>2.0f+1e-4f) {
            state->stable_since=now; state->stable_min=state->stable_max=polar;
        }
    }
}

bool desk_zero(desk *state, uint32_t now)
{
    desk_update(state,now);
    if (state->mode!=DESK_ADJUST || !state->angle_valid || !state->stable ||
        (uint32_t)(now-state->stable_since)<1000) return false;
    for(unsigned i=0;i<3;++i) state->reference[i]=state->direction[i];
    state->angle=0; state->tilt=false; state->confirming=false;
    return true;
}
