PYTHON ?= python3
CFLAGS ?= -std=c11 -O2 -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
SIMAVR_CFLAGS ?= -isystem $(shell pkg-config --variable=includedir simavr)/simavr
SIMAVR_LIBS ?= $(shell pkg-config --libs simavr)

.PHONY: all loader firmware generate test test-all test-sanitize clean
all: loader generate

loader: build/ueec-avr

build/ueec-avr: loader.c ueec_avr_metadata.h ueec_metadata.h reference/ueec.h reference/ueec_load.c
	mkdir -p build
	$(CC) $(CFLAGS) $(SIMAVR_CFLAGS) loader.c -o $@ $(SIMAVR_LIBS)

firmware:
	$(PYTHON) tools/build_firmware.py

generate:
	$(PYTHON) generate.py

test: all
	$(PYTHON) test_avr.py

# Rebuild all three firmwares; missing tools are errors, never skips.
test-all: firmware
	$(MAKE) test

test-sanitize: generate
	mkdir -p build
	$(CC) -std=c11 -O1 -g -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(SIMAVR_CFLAGS) loader.c -o build/ueec-avr-sanitize $(SIMAVR_LIBS)
	$(PYTHON) test_avr.py --loader build/ueec-avr-sanitize

clean:
	rm -rf build
