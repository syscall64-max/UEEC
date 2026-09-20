default: test.ueec align loader

test.ueec: 03_reloc.asm
	nasm -f bin 03_reloc.asm -o test.ueec

align: align.c ueec.h
	gcc -Wall -Wextra -O2 -o align align.c

loader: loader.c ueec.h
	gcc -Wall -Wextra -O2 -o loader loader.c

run-default: default
	./align test.ueec test_default.ueec
	./loader --L=monolithic test_default.ueec
	./loader --L=scattered test_default.ueec

run-g1: default
	./align test.ueec test_g1.ueec --granularity 1
	./loader --L=monolithic test_g1.ueec
	./loader --L=scattered test_g1.ueec

# ---------------- QEMU cross-architecture ----------------
define ARCH_TEST
$(1): default
	./align test.ueec test_default.ueec
	./align test.ueec test_g1.ueec --granularity 1
	$(2) -Wall -Wextra -O2 -static -o loader_$(1) loader.c
	$(3) ./loader_$(1) --L=monolithic test_default.ueec
	$(3) ./loader_$(1) --L=scattered test_default.ueec
	$(3) ./loader_$(1) --L=monolithic test_g1.ueec
	$(3) ./loader_$(1) --L=scattered test_g1.ueec
endef

$(eval $(call ARCH_TEST,arm32,arm-linux-gnueabihf-gcc,qemu-arm))
$(eval $(call ARCH_TEST,arm64,aarch64-linux-gnu-gcc,qemu-aarch64))
$(eval $(call ARCH_TEST,riscv64,riscv64-linux-gnu-gcc,qemu-riscv64))
$(eval $(call ARCH_TEST,riscv32,/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc,qemu-riscv32))
$(eval $(call ARCH_TEST,powerpc32,powerpc-linux-gnu-gcc,qemu-ppc))
$(eval $(call ARCH_TEST,powerpc64,powerpc64-linux-gnu-gcc,qemu-ppc64))
$(eval $(call ARCH_TEST,sparc32,sparc64-linux-gnu-gcc -m32,qemu-sparc32plus))
$(eval $(call ARCH_TEST,sparc64,sparc64-linux-gnu-gcc,qemu-sparc64))
$(eval $(call ARCH_TEST,longarm64,loongarch64-linux-gnu-gcc-14,qemu-loongarch64))
$(eval $(call ARCH_TEST,x86,i686-linux-gnu-gcc,qemu-i386))

x86_64: run-default run-g1

all: arm32 arm64 riscv64 powerpc32 powerpc64 sparc32 sparc64 longarm64 x86 x86_64 riscv32

clean:
	rm -f test.ueec test_default.ueec test_g1.ueec align loader \
	      loader_arm32 loader_arm64 loader_riscv64 loader_riscv32 \
	      loader_powerpc32 loader_powerpc64 loader_sparc32 loader_sparc64 loader_longarm64 loader_x86
