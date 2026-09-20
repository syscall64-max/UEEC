UEEC      := build/codes.ueec
BUILD_DIR := build

CFLAGS := -std=c11 -O2 -static \
	-Wall -Wextra -Wpedantic -Wconversion -Wshadow

CC_ARM32       ?= arm-linux-gnueabihf-gcc
CC_ARM64       ?= aarch64-linux-gnu-gcc
CC_LOONGARCH64 ?= loongarch64-linux-gnu-gcc-14
CC_POWERPC32   ?= powerpc-linux-gnu-gcc
CC_POWERPC64   ?= powerpc64-linux-gnu-gcc
CC_RISCV32     ?= /opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc
CC_RISCV64     ?= riscv64-linux-gnu-gcc
CC_SPARC32     ?= sparc64-linux-gnu-gcc
CC_SPARC64     ?= sparc64-linux-gnu-gcc
CC_X86         ?= i686-linux-gnu-gcc
CC_X86_64      ?= gcc

QEMU_ARM32       ?= qemu-arm
QEMU_ARM64       ?= qemu-aarch64
QEMU_LOONGARCH64 ?= qemu-loongarch64
QEMU_POWERPC32   ?= qemu-ppc
QEMU_POWERPC64   ?= qemu-ppc64
QEMU_RISCV32     ?= qemu-riscv32
QEMU_RISCV64     ?= qemu-riscv64
QEMU_SPARC32     ?= qemu-sparc32plus
QEMU_SPARC64     ?= qemu-sparc64
QEMU_X86         ?= qemu-i386

ARCHES := arm32 arm64 loongarch64 powerpc32 powerpc64 riscv32 riscv64 \
	sparc32 sparc64 x86 x86_64
BINARIES := $(addprefix $(BUILD_DIR)/ueec-load-,$(ARCHES))
BUILD_TARGETS := $(addprefix build-,$(ARCHES))
TEST_TARGETS := $(addprefix test-,$(ARCHES))

.PHONY: all build test ueec native test-modes $(BUILD_TARGETS) \
	$(TEST_TARGETS) clean

all: test

build: $(BINARIES)
	@echo "UEEC loader: 11 architecture builds PASS"

test: $(TEST_TARGETS)
	@echo "UEEC loader: 11 architecture tests PASS"

ueec: $(UEEC)

native: $(UEEC) $(BUILD_DIR)/ueec-load-x86_64
	$(BUILD_DIR)/ueec-load-x86_64 --node exit $(UEEC)

test-modes: $(UEEC) $(BUILD_DIR)/ueec-load-x86_64
	$(BUILD_DIR)/ueec-load-x86_64 --node exit --load-mode auto --no-run $(UEEC)
	$(BUILD_DIR)/ueec-load-x86_64 --node exit --load-mode monolithic --no-run $(UEEC)
	$(BUILD_DIR)/ueec-load-x86_64 --node exit --load-mode scattered --no-run $(UEEC)
	$(BUILD_DIR)/ueec-load-x86_64 --node exit --load-mode hybrid --no-run $(UEEC)
	@echo "UEEC loader: four load modes PASS"

$(BUILD_DIR)/.dir:
	mkdir -p $(BUILD_DIR)
	touch $@

$(UEEC): ueec.asm | $(BUILD_DIR)/.dir
	nasm -f bin $< -o $@

$(BUILD_DIR)/ueec-load-arm32: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_ARM32) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-arm64: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_ARM64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-loongarch64: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_LOONGARCH64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-powerpc32: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_POWERPC32) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-powerpc64: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_POWERPC64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-riscv32: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_RISCV32) $(CFLAGS) -march=rv32gc -mabi=ilp32d $< -o $@

$(BUILD_DIR)/ueec-load-riscv64: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_RISCV64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-sparc32: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_SPARC32) $(CFLAGS) -m32 $< -o $@

$(BUILD_DIR)/ueec-load-sparc64: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_SPARC64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-x86: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_X86) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-load-x86_64: ueec_load.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_X86_64) $(CFLAGS) $< -o $@

$(BUILD_TARGETS): build-%: $(BUILD_DIR)/ueec-load-%
	@echo "$*: build PASS"

test-arm32: $(UEEC) $(BUILD_DIR)/ueec-load-arm32
	$(QEMU_ARM32) $(BUILD_DIR)/ueec-load-arm32 --node exit $(UEEC)

test-arm64: $(UEEC) $(BUILD_DIR)/ueec-load-arm64
	$(QEMU_ARM64) $(BUILD_DIR)/ueec-load-arm64 --node exit $(UEEC)

test-loongarch64: $(UEEC) $(BUILD_DIR)/ueec-load-loongarch64
	$(QEMU_LOONGARCH64) $(BUILD_DIR)/ueec-load-loongarch64 --node exit $(UEEC)

test-powerpc32: $(UEEC) $(BUILD_DIR)/ueec-load-powerpc32
	$(QEMU_POWERPC32) $(BUILD_DIR)/ueec-load-powerpc32 --node exit $(UEEC)

test-powerpc64: $(UEEC) $(BUILD_DIR)/ueec-load-powerpc64
	$(QEMU_POWERPC64) $(BUILD_DIR)/ueec-load-powerpc64 --node exit $(UEEC)

test-riscv32: $(UEEC) $(BUILD_DIR)/ueec-load-riscv32
	$(QEMU_RISCV32) $(BUILD_DIR)/ueec-load-riscv32 --node exit $(UEEC)

test-riscv64: $(UEEC) $(BUILD_DIR)/ueec-load-riscv64
	$(QEMU_RISCV64) $(BUILD_DIR)/ueec-load-riscv64 --node exit $(UEEC)

test-sparc32: $(UEEC) $(BUILD_DIR)/ueec-load-sparc32
	$(QEMU_SPARC32) $(BUILD_DIR)/ueec-load-sparc32 --node exit $(UEEC)

test-sparc64: $(UEEC) $(BUILD_DIR)/ueec-load-sparc64
	$(QEMU_SPARC64) $(BUILD_DIR)/ueec-load-sparc64 --node exit $(UEEC)

test-x86: $(UEEC) $(BUILD_DIR)/ueec-load-x86
	$(QEMU_X86) $(BUILD_DIR)/ueec-load-x86 --node exit $(UEEC)

test-x86_64: $(UEEC) $(BUILD_DIR)/ueec-load-x86_64
	$(BUILD_DIR)/ueec-load-x86_64 --node exit $(UEEC)

clean:
	rm -rf -- $(BUILD_DIR)
