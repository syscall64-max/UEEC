PYTHON ?= python3
CFLAGS ?= -std=c11 -O2 -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror

.PHONY: all loader firmware generate setup-renode test test-all test-sanitize clean
all: loader generate
loader: build/ueec-stm32
build/ueec-stm32: loader.c ueec_stm32_metadata.h ueec_metadata.h reference/ueec.h reference/ueec_load.c
	mkdir -p build
	$(CC) $(CFLAGS) loader.c -o $@
firmware:
	$(PYTHON) tools/build_firmware.py
generate:
	$(PYTHON) generate.py
setup-renode:
	$(PYTHON) tools/setup_renode.py
test: all
	$(PYTHON) test_stm32.py
test-all: firmware
	$(MAKE) test
test-sanitize: generate
	mkdir -p build
	$(CC) -std=c11 -O1 -g -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer loader.c -o build/ueec-stm32-sanitize
	$(PYTHON) test_stm32.py --loader build/ueec-stm32-sanitize
clean:
	rm -rf build
