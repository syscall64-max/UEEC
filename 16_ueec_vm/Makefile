CC ?= cc
PYTHON ?= python3
CPPFLAGS += -D_DEFAULT_SOURCE
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow
BUILD := build
# LeakSanitizer cannot run under this workspace's ptrace supervisor. On an
# unrestricted host: make sanitize ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
ASAN_OPTIONS ?= detect_leaks=0:halt_on_error=1

.PHONY: all run list test sanitize clean
all: $(BUILD)/ueec-vm $(BUILD)/ueec-info $(BUILD)/ueec-info-original $(BUILD)/programs.ueec

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/ueec-vm: ueec_vm.c vm_runtime.c vm_runtime.h ueec.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) ueec_vm.c vm_runtime.c $(LDFLAGS) -o $@

$(BUILD)/ueec-info: ueec_info.c ueec.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) ueec_info.c $(LDFLAGS) -o $@

$(BUILD)/ueec-info-original: reference/ueec_info_original.c ueec.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. $< $(LDFLAGS) -o $@

$(BUILD)/programs.ueec: generate.py asm_subset.py examples/stack16.asm | $(BUILD)
	$(PYTHON) generate.py --output $@

list: all
	$(BUILD)/ueec-vm --list $(BUILD)/programs.ueec

run: all
	$(BUILD)/ueec-vm --node hello $(BUILD)/programs.ueec
	$(BUILD)/ueec-vm --node stack $(BUILD)/programs.ueec

test: all
	$(PYTHON) test_vm.py --binary $(BUILD)/ueec-vm

sanitize: $(BUILD)/ueec-info $(BUILD)/ueec-info-original $(BUILD)/programs.ueec
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie ueec_vm.c vm_runtime.c -o $(BUILD)/ueec-vm-san
	ASAN_OPTIONS=$(ASAN_OPTIONS) UBSAN_OPTIONS=halt_on_error=1 $(PYTHON) test_vm.py --binary $(BUILD)/ueec-vm-san

clean:
	rm -rf $(BUILD)
