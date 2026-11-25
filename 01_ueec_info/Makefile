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
CC_S390X       ?= s390x-linux-gnu-gcc

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
QEMU_S390X       ?= qemu-s390x

ARCHES := arm32 arm64 loongarch64 powerpc32 powerpc64 riscv32 riscv64 \
	sparc32 sparc64 x86 x86_64 s390x

BINARIES := $(addprefix $(BUILD_DIR)/ueec-info-,$(ARCHES))
BUILD_TARGETS := $(addprefix build-,$(ARCHES))
TEST_TARGETS  := $(addprefix test-,$(ARCHES))

.PHONY: all build test ueec native test-layout $(BUILD_TARGETS) $(TEST_TARGETS) clean

all: test

build: $(BINARIES)
	@echo "UEEC info: 12 architecture builds PASS"

test: $(TEST_TARGETS) test-layout
	@echo "UEEC info: 12 architecture tests and layout check PASS"

ueec: $(UEEC)

native: $(UEEC) $(BUILD_DIR)/ueec-info-x86_64
	$(BUILD_DIR)/ueec-info-x86_64 $(UEEC)

$(BUILD_DIR)/.dir:
	mkdir -p $(BUILD_DIR)
	touch $@

$(UEEC): ueec.asm | $(BUILD_DIR)/.dir
	nasm -f bin $< -o $@

$(BUILD_DIR)/ueec-info-arm32: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_ARM32) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-arm64: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_ARM64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-loongarch64: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_LOONGARCH64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-powerpc32: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_POWERPC32) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-powerpc64: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_POWERPC64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-riscv32: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_RISCV32) $(CFLAGS) -march=rv32gc -mabi=ilp32d $< -o $@

$(BUILD_DIR)/ueec-info-riscv64: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_RISCV64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-sparc32: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_SPARC32) $(CFLAGS) -m32 $< -o $@

$(BUILD_DIR)/ueec-info-sparc64: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_SPARC64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-x86: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_X86) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-x86_64: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_X86_64) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ueec-info-s390x: ueec_info.c ueec.h | $(BUILD_DIR)/.dir
	$(CC_S390X) $(CFLAGS) $< -o $@

$(BUILD_TARGETS): build-%: $(BUILD_DIR)/ueec-info-%
	@echo "$*: build PASS"

test-arm32: $(UEEC) $(BUILD_DIR)/ueec-info-arm32
	$(QEMU_ARM32) $(BUILD_DIR)/ueec-info-arm32 $(UEEC)

test-arm64: $(UEEC) $(BUILD_DIR)/ueec-info-arm64
	$(QEMU_ARM64) $(BUILD_DIR)/ueec-info-arm64 $(UEEC)

test-loongarch64: $(UEEC) $(BUILD_DIR)/ueec-info-loongarch64
	$(QEMU_LOONGARCH64) $(BUILD_DIR)/ueec-info-loongarch64 $(UEEC)

test-powerpc32: $(UEEC) $(BUILD_DIR)/ueec-info-powerpc32
	$(QEMU_POWERPC32) $(BUILD_DIR)/ueec-info-powerpc32 $(UEEC)

test-powerpc64: $(UEEC) $(BUILD_DIR)/ueec-info-powerpc64
	$(QEMU_POWERPC64) $(BUILD_DIR)/ueec-info-powerpc64 $(UEEC)

test-riscv32: $(UEEC) $(BUILD_DIR)/ueec-info-riscv32
	$(QEMU_RISCV32) $(BUILD_DIR)/ueec-info-riscv32 $(UEEC)

test-riscv64: $(UEEC) $(BUILD_DIR)/ueec-info-riscv64
	$(QEMU_RISCV64) $(BUILD_DIR)/ueec-info-riscv64 $(UEEC)

test-sparc32: $(UEEC) $(BUILD_DIR)/ueec-info-sparc32
	$(QEMU_SPARC32) $(BUILD_DIR)/ueec-info-sparc32 $(UEEC)

test-sparc64: $(UEEC) $(BUILD_DIR)/ueec-info-sparc64
	$(QEMU_SPARC64) $(BUILD_DIR)/ueec-info-sparc64 $(UEEC)

test-x86: $(UEEC) $(BUILD_DIR)/ueec-info-x86
	$(QEMU_X86) $(BUILD_DIR)/ueec-info-x86 $(UEEC)

test-x86_64: $(UEEC) $(BUILD_DIR)/ueec-info-x86_64
	$(BUILD_DIR)/ueec-info-x86_64 $(UEEC)

test-s390x: $(UEEC) $(BUILD_DIR)/ueec-info-s390x
	$(QEMU_S390X) $(BUILD_DIR)/ueec-info-s390x $(UEEC)

# Native structural assertions: count, node identification, shared section map.
test-layout: $(UEEC) $(BUILD_DIR)/ueec-info-x86_64
	@set -e; output="$$($(BUILD_DIR)/ueec-info-x86_64 $(UEEC))"; \
	printf '%s\n' "$$output" | grep -Eq '^[[:space:]]*nodes:[[:space:]]*12$$'; \
	printf '%s\n' "$$output" | grep -Eq '^[[:space:]]*global sections:[[:space:]]*13$$'; \
	printf '%s\n' "$$output" | grep -Fq '[11] name="exit" machine=CPU/s390x system=general-os/Linux code-endian=big'; \
	printf '%s\n' "$$output" | grep -Fq '[12] type=CODE offset=391 section_size=8 data_size=8 nodes=11'; \
	echo "UEEC info: 12-node layout PASS"

clean:
	rm -f $(UEEC) $(BINARIES) $(BUILD_DIR)/.dir
	-rmdir $(BUILD_DIR)
