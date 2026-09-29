CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g
BUILD = build/$(shell uname -s)-$(shell uname -m)
.PHONY: test clean
test: $(BUILD)/driver $(BUILD)/desk
	$(BUILD)/driver
	$(BUILD)/desk
$(BUILD)/driver: test_driver.c ../firmware/Sensors/imu.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/desk: test_desk.c ../firmware/Desk/desk.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
clean:
	rm -rf build coverage
