CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
LDFLAGS ?= -static
ARCHES = x86 x86_64 arm32 aarch64 riscv32 riscv64 ppc32 ppc64 sparc32 sparc64 loongarch64 s390x
.PHONY: test sanitize clean $(ARCHES) $(addprefix payload-,$(ARCHES))
test: $(ARCHES)
	python3 report.py
.PHONY: prepare
prepare: $(addprefix payload-,$(ARCHES))
	python3 test.py prepare
x86 payload-x86: CC_ARCH=i686-linux-gnu-gcc
x86: RUNNER=qemu-i386
x86_64 payload-x86_64: CC_ARCH=gcc
arm32 payload-arm32: CC_ARCH=arm-linux-gnueabihf-gcc
arm32: RUNNER=qemu-arm
aarch64 payload-aarch64: CC_ARCH=aarch64-linux-gnu-gcc
aarch64: RUNNER=qemu-aarch64
riscv32 payload-riscv32: CC_ARCH=/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc -march=rv32gc -mabi=ilp32d
riscv32: RUNNER=qemu-riscv32
riscv64 payload-riscv64: CC_ARCH=riscv64-linux-gnu-gcc
riscv64: RUNNER=qemu-riscv64
ppc32 payload-ppc32: CC_ARCH=powerpc-linux-gnu-gcc
ppc32: RUNNER=qemu-ppc
ppc64 payload-ppc64: CC_ARCH=powerpc64-linux-gnu-gcc
ppc64: RUNNER=qemu-ppc64
sparc32 payload-sparc32: CC_ARCH=sparc64-linux-gnu-gcc -m32
sparc32: RUNNER=qemu-sparc32plus
sparc64 payload-sparc64: CC_ARCH=sparc64-linux-gnu-gcc
sparc64: RUNNER=qemu-sparc64
loongarch64 payload-loongarch64: CC_ARCH=loongarch64-linux-gnu-gcc-15
loongarch64: RUNNER=qemu-loongarch64
s390x payload-s390x: CC_ARCH=s390x-linux-gnu-gcc
s390x: RUNNER=qemu-s390x
$(addprefix payload-,$(ARCHES)):
	mkdir -p build/payloads/$(patsubst payload-%,%,$@)
	@set -e; for v in 0 42 255; do \
	  $(CC_ARCH) -DVALUE=$$v -c payload.S -o build/payloads/$(patsubst payload-%,%,$@)/$$v.o; \
	  python3 extract_text.py build/payloads/$(patsubst payload-%,%,$@)/$$v.o build/payloads/$(patsubst payload-%,%,$@)/$$v.bin; \
	done
$(ARCHES): prepare
	mkdir -p build/$@
	$(CC_ARCH) $(CFLAGS) load.c $(LDFLAGS) -o build/$@/load
	python3 test.py $@ build/$@/load '$(RUNNER)'
sanitize:
	$(MAKE) x86_64 CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie' LDFLAGS=''
clean:
	rm -rf build
