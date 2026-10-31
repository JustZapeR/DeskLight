CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -g
BUILD = build/$(shell uname -s)-$(shell uname -m)
.PHONY: test clean
test: $(BUILD)/systick_time $(BUILD)/clock_fold $(BUILD)/worklog $(BUILD)/driver $(BUILD)/desk $(BUILD)/tilt $(BUILD)/console $(BUILD)/faults
	$(BUILD)/systick_time
	$(BUILD)/clock_fold
	$(BUILD)/worklog
	$(BUILD)/driver
	$(BUILD)/desk
	$(BUILD)/tilt
	$(BUILD)/console
	$(BUILD)/faults
$(BUILD)/driver: test_driver.c ../firmware/Sensors/imu.c fixtures/mpu-captured.h ../firmware/Sensors/imu.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(filter %.c,$^) -lm -o $@
$(BUILD)/desk: test_desk.c ../firmware/Desk/desk.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/tilt: test_tilt.c ../firmware/Desk/desk.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/console: test_console.c ../firmware/Desk/desk.c ../firmware/Desk/console.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/faults: test_faults.c ../firmware/Desk/desk.c ../firmware/Desk/console.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
clean:
	rm -rf build coverage

$(BUILD)/worklog: test_worklog.c ../firmware/Desk/worklog.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/clock_fold: test_clock_fold.c ../firmware/Board/clock_fold.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/systick_time: test_systick_time.c ../firmware/Board/systick_time.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< -o $@
