CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g
BUILD = build/$(shell uname -s)-$(shell uname -m)
.PHONY: test clean
test: $(BUILD)/driver $(BUILD)/desk $(BUILD)/tilt
	$(BUILD)/driver
	$(BUILD)/desk
	$(BUILD)/tilt
$(BUILD)/driver: test_driver.c ../firmware/Sensors/imu.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/desk: test_desk.c ../firmware/Desk/desk.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/tilt: test_tilt.c ../firmware/Desk/desk.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
clean:
	rm -rf build coverage
