#include "wokwi-api.h"
/* Test stimulus, not part of the DeskLight firmware. */
static pin_t marker, input;
static timer_t timer;
static enum { READY, WAIT_HIGH, PULSE, REFRACTORY } phase;
static void tick(void *unused) {
  (void)unused;
  if (phase == WAIT_HIGH) {
    if (pin_read(marker) == HIGH) {
      pin_write(input, HIGH); phase = PULSE; timer_start(timer, 60000, false);
    } else phase = READY;
  } else if (phase == PULSE) {
    pin_write(input, LOW); phase = REFRACTORY; timer_start(timer, 100000, false);
  } else if (phase == REFRACTORY) phase = READY;
}
static void marker_changed(void *unused, pin_t pin, uint32_t value) {
  (void)unused; (void)pin;
  if (value == HIGH && phase == READY) { phase = WAIT_HIGH; timer_start(timer, 100, false); }
}
void chip_init(void) {
  marker = pin_init("MARK", INPUT); input = pin_init("PIR", OUTPUT_LOW); phase = READY;
  const timer_config_t tc = {.callback = tick, .user_data = 0};
  timer = timer_init(&tc);
  const pin_watch_config_t wc = {.edge = RISING, .pin_change = marker_changed, .user_data = 0};
  pin_watch(marker, &wc);
}
