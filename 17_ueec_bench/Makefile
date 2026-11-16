CC ?= cc
PYTHON ?= python3
CPPFLAGS += -D_DEFAULT_SOURCE
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow
ASAN_OPTIONS ?= detect_leaks=0:halt_on_error=1
BUILD := build
VM := ../16_ueec_vm
DEPS := Makefile bench_load.c $(VM)/ueec_vm.c $(VM)/vm_runtime.c $(VM)/vm_runtime.h $(VM)/ueec.h

.PHONY: all bench quick test sanitize clean
all: $(BUILD)/bench-load

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/bench-load: $(DEPS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) bench_load.c $(VM)/vm_runtime.c $(LDFLAGS) -o $@
	$(file >$(BUILD)/build-command.txt,$(CC) $(CPPFLAGS) $(CFLAGS) bench_load.c $(VM)/vm_runtime.c $(LDFLAGS) -o $@)

bench: all
	$(PYTHON) benchmark.py --binary $(BUILD)/bench-load

quick: all
	$(PYTHON) benchmark.py --binary $(BUILD)/bench-load --quick

test: all
	$(PYTHON) test_benchmark.py --binary $(BUILD)/bench-load

sanitize: | $(BUILD)
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie bench_load.c $(VM)/vm_runtime.c -o $(BUILD)/bench-load-san
	ASAN_OPTIONS=$(ASAN_OPTIONS) UBSAN_OPTIONS=halt_on_error=1 $(PYTHON) test_benchmark.py --binary $(BUILD)/bench-load-san

clean:
	rm -rf $(BUILD)
