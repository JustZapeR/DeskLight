#include "console.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

desk_command console_feed(desk_console *console, char byte)
{
    if (byte != '\r' && byte != '\n') {
        if (console->length+1 < sizeof console->line)
            console->line[console->length++]=byte;
        else console->overflow=true;
        return CMD_NONE;
    }
    if (!console->length && !console->overflow) return CMD_NONE;
    console->line[console->length]=0;
    static const struct { const char *text; desk_command command; } commands[] = {
        {"mode adjust",CMD_ADJUST}, {"mode auto",CMD_AUTO}, {"zero",CMD_ZERO},
        {"sample",CMD_SAMPLE}, {"probe nack",CMD_NACK},
        {"fault imu on",CMD_IMU_OFF}, {"fault imu off",CMD_IMU_ON},
        {"fault adc on",CMD_ADC_OFF}, {"fault adc off",CMD_ADC_ON},
        {"pause imu on",CMD_IMU_PAUSE}, {"pause imu off",CMD_IMU_RESUME},
        {"pause adc on",CMD_ADC_PAUSE}, {"pause adc off",CMD_ADC_RESUME}
    };
    desk_command result=CMD_UNKNOWN;
    if (!console->overflow)
        for(unsigned i=0;i<sizeof commands/sizeof commands[0];++i)
            if (strcmp(console->line,commands[i].text)==0) result=commands[i].command;
    console->length=0; console->overflow=false;
    return result;
}

const char *console_apply(desk *state, desk_command command, uint32_t now)
{
    switch(command) {
    case CMD_ADJUST: desk_adjust(state); return "mode adjust ok";
    case CMD_AUTO: desk_auto(state); return "mode auto ok";
    case CMD_ZERO: return desk_zero(state,now) ? "zero accepted" : "zero rejected";
    case CMD_NONE: return "";
    case CMD_UNKNOWN: return "unknown command";
    default: return "diagnostic";
    }
}

bool led_update(desk_led *led, const desk *state, uint32_t now)
{
    unsigned priority=state->fault ? 3 : state->tilt ? 2 : state->need_light ? 1 : 0;
    if (priority!=led->priority) {
        led->priority=priority; led->changed=now; led->level=priority!=0;
    }
    unsigned period=priority==3 ? 100 : priority==2 ? 250 : 0;
    if (period) led->level=((uint32_t)(now-led->changed)/period % 2)==0;
    else led->level=priority==1;
    return led->level;
}

size_t desk_status(const desk *s, uint32_t now, char *output, size_t capacity)
{
    char angle[24], norm[24];
    if (s->angle_valid && isfinite(s->angle) && s->angle>=0 && s->angle<=180) {
        unsigned millideg=(unsigned)(s->angle*1000+.5f);
        snprintf(angle,sizeof angle,"%u.%03u",millideg/1000,millideg%1000);
    } else strcpy(angle,"invalid");
    if (isfinite(s->norm) && s->norm>=0 && s->norm<4294967.0f)
        snprintf(norm,sizeof norm,"%u",(unsigned)(s->norm*1000+.5f));
    else strcpy(norm,"invalid");
    int n=snprintf(output,capacity,
        "STATUS t=%lu mode=%s pir=%u pir_valid=%u raw=%u avg=%lu.%lu light_valid=%u "
        "dark=%u angle=%s imu_valid=%u norm_mg=%s need_light=%u tilt=%u fault=%u\r\n",
        (unsigned long)now,desk_mode_name(s->mode),s->pir,s->pir_health.valid,s->raw,
        (unsigned long)(s->sum/5),(unsigned long)(s->sum%5*2),s->light_valid,
        s->dark,angle,s->imu_health.valid,norm,
        s->need_light,s->tilt,s->fault);
    return n>0 && (size_t)n<capacity ? (size_t)n : 0;
}
