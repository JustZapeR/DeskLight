IMAGE ?= stm32c0-wokwi-build:local

.PHONY: image build clean test coverage sanitize

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware all

clean:
	$(MAKE) -C firmware clean

test:
	python3 tools/check_project.py
	$(MAKE) -C tests test

coverage:
	python3 tools/coverage.py

sanitize:
	$(MAKE) -C tests test BUILD=build/sanitize CFLAGS="-std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer"
